
#include "tool/util.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/FileManager.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <clang/Tooling/CompilationDatabase.h>
#pragma GCC diagnostic pop

#include <CLI/CLI.hpp>
#include <CLI/Error.hpp>
#include <CLI/Validators.hpp>

#include <algorithm>
#include <expected>
#include <filesystem>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

struct Context {
  clang::FileManager file_manager;
  clang::DiagnosticsEngine diagnostics_engine;
};

/*
 * [(source_file, [flags], [opts])]
 * | parse_ast(cached_ctx) -> ast
 * | fold_left(accum, matchers)
 */

namespace {
namespace cli {

template <typename T>
constexpr auto str_by = {};

template <typename T>
constexpr std::expected<T, std::string> from_string(std::string_view v) {
  if (const auto &found = std::ranges::find_if(str_by<T>,
        [v](const auto &key_val) { return v == key_val.first; });
    found != str_by<T>.cend())
    return found->second;

  return std::unexpected(std::format("Invalid enum value '{}'", v));
}

template <typename T>
  requires std::is_enum_v<T>
constexpr std::string_view to_string(const T &v) {
  if (const auto &found = std::ranges::find_if(str_by<T>,
        [v](const auto &keyValue) { return v == keyValue.second; });
    str_by<T>.cend() != found)
    return found->first;
  return "unknown";
}

struct source_file {
  fs::path path;

  // deduplicate if compilation_db contains several commands for one file
  std::string output_substr;
};

// CLI11 uses ADL to specialize parsing cli args to custom types
bool lexical_cast(const std::string &arg, source_file &src) {
  std::ranges::view auto v_parts = std::views::split(arg, ':')
    | std::views::transform([](const auto &s) { return std::string_view(s); });
  const std::vector parts(v_parts.begin(), v_parts.end());
  if (2 < parts.size() || parts.front().empty())
    return false;

  src.path = parts.front();
  src.output_substr = 2 == parts.size() ? parts.back() : "";

  return true;
}

// compilation flags
struct cl_flags {
  std::vector<std::string_view> values;
};

// compile_commands.json
struct json_cl_db {
  fs::path path;
};

// todo: would be cool to use the tool itself to parse the cli commands directly
// into the struct
struct options {
  // .cpp sources
  std::vector<source_file> sources;

  // source mode: path for output file (may include the file name)
  // header mode: output dir
  fs::path out;

  // flags or compilation db to get the flags from
  std::variant<cl_flags, json_cl_db> flags;

  // Deduced from the installation path if nullopt
  std::optional<fs::path> resource_dir;

  enum mode_t {
    header,
    source,
    dump,
  } mode;
};

template <>
constexpr std::array str_by<options::mode_t> = std::invoke([] {
  using namespace std::string_view_literals;

  auto map = std::array{
    std::pair{"header"sv, options::header},
    std::pair{"source"sv, options::source},
    std::pair{"dump"sv, options::dump},
  };
  std::ranges::sort(map,
    [](const auto &lhs, const auto &rhs) { return lhs.first < rhs.first; });
  return map;
});

std::string to_string(const options &o) {
  return fmt::format(R"(mode:{mode}
sources:[{sources}]
out:{out}
resource_dir:{resource_dir})",
    fmt::arg("mode", to_string(o.mode)),
    fmt::arg("sources",
      util::join(o.sources,
        ",\n",
        [](const source_file &s, fmt::context &ctx) {
          return fmt::format_to(ctx.out(), "{}", s.path.string());
        })),
    fmt::arg("out", o.out.generic_string()),
    fmt::arg("resource_dir", o.resource_dir.value_or("").generic_string()));
}

std::expected<options, std::pair<int, std::string>> parse(int argc,
  const char *const *argv) noexcept {
  CLI::App app{
    R"(
C++ reflection code generator that operates in three modes:

Source Mode (default):
  Generates a single .cpp file containing reflected call implementations for a 
  list of .cpp sources. Compiled object file needs to be linked to the resulting binary.

Header Mode:
  Generates .hpp header files containing reflected call implementations for 
  given .cpp files. Headers must be implicitly included at the start of each 
  translation unit.
  WARNING: Uses compile time counters via friend injection, which is not guaranteed
           by the C++ Standard to be consistent between compiler implementations.

Dump yaml:
  todo: explain
)"};

  options opts{};

  app
    .add_option("--resource-dir",
      opts.resource_dir,
      "Path to bundled system headers.")
    ->type_name("PATH")
    ->check(CLI::ExistingDirectory);
  // ->default_val(fs::path()); //< todo: resolve from installation

  // todo: how to print normal error?
  app.add_option("--mode", opts.mode, "reflection mode")
    ->default_val(cli::options::source)
    ->transform(CLI::CheckedTransformer(cli::str_by<cli::options::mode_t>));

  app
    .add_option("sources",
      opts.sources,
      "List of .cpp file paths to run the tool on.")
    ->type_name("PATH");

  app
    .add_option("-o,--out", opts.out, "output directory (may contain filename)")
    ->default_val(fs::current_path())
    ->type_name("PATH");

  // Validation here
  app.callback([&] {
    std::ranges::for_each(opts.sources, [](source_file &s) {
      s.path = fs::absolute(std::move(s.path)).lexically_normal();
    });

    std::ranges::view auto invalid_sources = opts.sources
      | std::views::filter(
        [](const source_file &s) -> bool { return !fs::exists(s.path); });

    if (!invalid_sources.empty()) {
      throw CLI::ValidationError("Non-existent sources:",
        fmt::format("[{}]",
          util::join(
            std::vector(invalid_sources.begin(), invalid_sources.end()),
            ", ",
            [](const source_file &s, fmt::context &ctx) {
              return fmt::format_to(ctx.out(), "{}", s.path.string());
            })));
    }

    if (auto *comp_db = std::get_if<json_cl_db>(&opts.flags)) {
      comp_db->path = fs::absolute(std::move(comp_db->path)).lexically_normal();

      if (!fs::exists(comp_db->path))
        throw CLI::ValidationError("Invalid compilation db path:",
          comp_db->path.generic_string());

      const std::expected loaded = std::invoke(
        [](const fs::path &db_path)
          -> std::expected<std::unique_ptr<clang::tooling::CompilationDatabase>,
            std::string> {
          std::string err;
          std::unique_ptr loaded =
            clang::tooling::CompilationDatabase::loadFromDirectory(
              db_path.generic_string(),
              err);

          if (!loaded)
            return std::unexpected(std::move(err));

          return loaded;
        },
        comp_db->path);

      if (!loaded)
        throw CLI::ValidationError("Invalid compilation db file:",
          comp_db->path.generic_string());

      // todo:
      //   - all sources are found within the compilation db
      //   - if db contains more than one source, check that disambiguation is
      //   provided and found within the db
    }
    // todo: implement
  });

  // todo: implement
  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    std::stringstream ss;
    const auto code = app.exit(e, ss, ss);
    return std::unexpected(std::pair{code, std::move(ss).str()});
  }

  return opts;
}

} // namespace cli
} // namespace

int main(int argc, char **argv) {
  const std::expected cli_args = cli::parse(argc, argv);
  if (!cli_args) {
    const auto &[code, error] = cli_args.error();
    std::cerr << error << '\n';
    return code;
  }

  fmt::println("args:\n{}", to_string(*cli_args));

  return 0;
}
