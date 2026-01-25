#include "tool/cli.hpp"
#include "tool/util.hpp"

#include <CLI/CLI.hpp>
#include <CLI/Error.hpp>

#include <algorithm>
#include <expected>
#include <filesystem>
#include <ranges>
#include <sstream>
#include <tuple>

#include <fmt/core.h>

namespace {

constexpr std::array map_str_verbosity = std::invoke([] {
  using _p = std::pair<std::string_view, tool::cli::verbosity_level>;
  std::array m{
    _p{"debug", tool::cli::verbosity_level::debug},
    _p{"info", tool::cli::verbosity_level::info},
    _p{"input", tool::cli::verbosity_level::input},
    _p{"parsed_types", tool::cli::verbosity_level::parsed_types},
  };
  std::ranges::sort(m,
    [](const auto &lhs, const auto &rhs) { return lhs.first < rhs.first; });
  return m;
});

template <typename Key, typename Val, size_t N>
consteval std::array<std::pair<Val, Key>, N> inverted(
  const std::array<std::pair<Key, Val>, N> &table) noexcept {
  return std::apply(
    [](const auto &...key_val) {
      return std::array{std::pair{key_val.second, key_val.first}...};
    },
    table);
}

} // namespace

std::string tool::cli::to_string(verbosity_level v) noexcept {
  // todo: use table
  constexpr auto table = ::inverted(::map_str_verbosity);
  (void)table;
  // todo: static_assert if `v` is not stringifiable

  // todo: (?) assert
  constexpr std::string_view s_debug = "debug";
  constexpr std::string_view s_input = "input";
  constexpr std::string_view s_info = "info";
  constexpr std::string_view s_parsed_types = "parsed_types";

  if (!v)
    return "none";

  std::vector<std::string_view> values;
  if ((verbosity_level::debug & v) == verbosity_level::debug)
    values.emplace_back(s_debug);

  if (verbosity_level::input & v)
    values.emplace_back(s_input);

  if (verbosity_level::info & v)
    values.emplace_back(s_info);

  if (verbosity_level::parsed_types & v)
    values.emplace_back(s_parsed_types);

  return fmt::format("{}", fmt::join(values, "|"));
}

std::expected<tool::cli::verbosity_level, std::monostate>
  tool::cli::from_string(std::string_view s) noexcept {
  const auto found = std::find_if(::map_str_verbosity.cbegin(),
    ::map_str_verbosity.cend(),
    [s](const auto &p) { return p.first == s; });
  if (found != ::map_str_verbosity.cend())
    return found->second;

  return std::unexpected(std::monostate());
}

std::string tool::cli::to_string(const options &o) {
  std::string mode = std::visit(
    [](const auto &_m) -> std::string {
      using mode_type = std::decay_t<decltype(_m)>;
      if constexpr (std::is_same_v<source_mode, mode_type>) {
        const source_mode &m = _m;
        return fmt::format(
          R"(source-mode,
--output-file={},
--output-dir={})",
          m.output_file.string(),
          m.output_dir.string());
      } else {
        const header_mode &m = _m;
        return fmt::format(
          R"(header-mode,
--output-dir={})",
          m.output_dir.string());
      }
    },
    o.mode);

  std::string cl_flags = std::visit(
    [](const auto &_v) -> std::string {
      using type = std::decay_t<decltype(_v)>;
      if constexpr (std::is_same_v<compilation_db_entry, type>) {
        const compilation_db_entry &v = _v;
        return fmt::format("--compilation-db={},", v.path.string());
      } else {
        const std::vector<std::string> &v = _v;
        return fmt::format("--cl-flags=[{}]", fmt::join(v, " "));
      }
    },
    o.cl_flags);

  return fmt::format(R"({mode},
sources=[{sources}],
{cl_flags},
--resource-dir={resource_dir},
--verbosity={verbosity_level})",
    fmt::arg("mode", std::move(mode)),
    fmt::arg("sources",
      util::join(o.sources,
        ",\n",
        [](const std::filesystem::path &p, fmt::context &ctx) {
          return fmt::format_to(ctx.out(), "{}", p.string());
        })),
    fmt::arg("cl_flags", std::move(cl_flags)),
    fmt::arg("resource_dir", o.resource_dir.string()),
    fmt::arg("verbosity_level", to_string(o.verbosity)));
}

std::expected<tool::cli::options, std::pair<std::string, int>>
  tool::cli::parse(int argc, const char *const *argv) noexcept {
  CLI::App app{
    R"(
C++ reflection code generator that operates in two modes:

Source Mode (default):
  Generates a single .cpp file containing reflected call implementations for a 
  list of .cpp sources. Compiled object file needs to be linked to the resulting binary.

Header Mode:
  Generates .hpp header files containing reflected call implementations for 
  given .cpp files. Headers be implicitly included at the start of each 
  translation unit.
  WARNING: Uses compile time counters via friend injection, which is not guaranteed
           by the C++ Standard to be consistent between compiler implementations.
)"};

  // tool [--header-mode] [--target=<target_name>] [--compilation-db=<dir>]
  // [sources...] [-o,--out-file=<file>] [--output-dir=<dir>]
  bool cli_in_place_mode = false;
  const auto &opt_header_mode =
    app.add_flag("--header-mode", cli_in_place_mode)
      ->description("Use Header Mode");

  std::vector<std::filesystem::path> cli_sources;
  app
    .add_option("sources",
      cli_sources,
      "List of .cpp file paths for reflection.")
    ->type_name("PATH");

  // todo: clarify the type. I suppose it should go as `--cl-flags="-std=c++17
  // -O3", although I need a subset that is relevant only for correct AST
  // parsing
  std::vector<std::string> cli_cl_flags;
  const auto &opt_cli_cl_flags = app.add_option("--cl-flags",
    cli_cl_flags,
    "List of Clang-compatible compilation flags.");

  std::filesystem::path cli_compilation_db_dir;
  const auto &opt_compilation_db = //
    app
      .add_option<std::filesystem::path>("--compilation-db",
        cli_compilation_db_dir,
        "Path to 'compilie_commands.json' dir.")
      ->type_name("PATH")
      ->excludes(opt_cli_cl_flags);

  verbosity_level cli_verbosity = verbosity_level::none;
  app.add_flag_callback(
    "--debug",
    [&cli_verbosity] {
      cli_verbosity = cli_verbosity | verbosity_level::debug;
    },
    "Verbosity level: Debug. Print everything");

  app.add_flag_callback(
    "--info",
    [&cli_verbosity] { cli_verbosity = cli_verbosity | verbosity_level::info; },
    "Verbosity level: Info. Print status messages.");

  app.add_flag_callback(
    "--input",
    [&cli_verbosity] {
      cli_verbosity = cli_verbosity | verbosity_level::input;
    },
    "Verbosity level: Input. Print CLI input values.");

  app.add_flag_callback(
    "--parsed-types",
    [&cli_verbosity] {
      cli_verbosity = cli_verbosity | verbosity_level::parsed_types;
    },
    "Verbosity level: Parsed Types. Print reflected types.");

  app.callback([&]() {
    // all custom validation is here
    if (!cli_sources.empty()) {
      return;
    }

    if (!opt_cli_cl_flags->empty()) {
      throw CLI::ValidationError("sources",
        "At least one source must be specified when --cl-flags is used.");
    }

    if (opt_compilation_db->empty()) {
      throw CLI::ValidationError("--compilation-db",
        "Compilation DB is necessary if no sources are provided.");
    }
  });

  std::filesystem::path cli_output_dir;
  app
    .add_option("--output-dir",
      cli_output_dir,
      "Path to generate reflected sources to.")
    ->type_name("PATH");

  std::filesystem::path cli_output_file;
  app
    .add_option("--output-file",
      cli_output_file,
      "Filename for generated reflection implementation.")
    ->type_name("PATH")
    ->excludes(opt_header_mode);

  std::filesystem::path cli_resource_dir;
  app
    .add_option("--resource-dir",
      cli_resource_dir,
      "Path to bundled system headers.")
    ->type_name("PATH");

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    // ad hoc wrapper
    std::stringstream ss;
    const auto code = app.exit(e, ss, ss);
    return std::unexpected(std::pair{std::move(ss).str(), code});
  }

  using mode_type = decltype(options::mode);
  using cl_flags_type = decltype(options::cl_flags);
  return options{
    .mode = cli_in_place_mode //
      ? mode_type{header_mode{
          .output_dir = std::move(cli_output_dir),
        }}
      : mode_type{source_mode{
          .output_file = std::move(cli_output_file),
          .output_dir = std::move(cli_output_dir),
        }},
    .sources = std::move(cli_sources),
    .cl_flags = !opt_compilation_db->empty() //
      ? cl_flags_type{compilation_db_entry{
          .path = std::move(cli_compilation_db_dir),
        }}
      : cl_flags_type{std::move(cli_cl_flags)},
    .resource_dir = std::move(cli_resource_dir),
    .verbosity = cli_verbosity,
  };
}

std::expected<tool::cli::options, std::string> tool::cli::evaluate_defaults(
  options o) noexcept {
  // todo: evaluate (resource dir, whatever...)
  if (!o.resource_dir.empty()) {
    // todo:
  }

  // .mode
  std::visit(
    [](auto &_m) {
      if (_m.output_dir.empty())
        _m.output_dir = std::filesystem::current_path();

      using mode_type = std::decay_t<decltype(_m)>;
      if constexpr (std::is_same_v<source_mode, mode_type>) {
        source_mode &m = _m;
        if (m.output_file.empty())
          m.output_file = "reflected.cpp";
      }
    },
    o.mode);

  // todo: (? this might require complex logic) .resource_dir

  return o;
}
