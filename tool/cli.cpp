#include "tool/cli.hpp"

#include "CLI/Error.hpp"
#include "fmt/base.h"
#include "fmt/format.h"
#include "fmt/ranges.h"
#include "tool/util.hpp"

#include <CLI/CLI.hpp>

#include <filesystem>
#include <sstream>
#include <sys/types.h>

// refactorme: remove, use CLI11
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Error.h>
#pragma GCC diagnostic pop

#include <fmt/core.h>

namespace {
// clang-format off
const char help_message[] =
// todo: document all the parameters
    "\n"
    "-p <build-path> is used to read a compile command database.\n"
    "\n"
    "\tFor example, it can be a CMake build directory in which a file named\n"
    "\tcompile_commands.json exists (use -DCMAKE_EXPORT_COMPILE_COMMANDS=ON\n"
    "\tCMake option to get this output).\n"
    "\n"
    "[<source> ...] optionally specify the paths of source files. These paths are\n"
    "\tlooked up in the compile command database. If the path of a file is\n"
    "\tabsolute, it needs to point into CMake's source tree. If the path is\n"
    "\trelative, the current working directory needs to be in the CMake\n"
    "\tsource tree and the file must be in a subdirectory of the current\n"
    "\tworking directory. \"./\" prefixes in the relative files will be\n"
    "\tautomatically removed, but the rest of a relative path must be a\n"
    "\tsuffix of a path in the compile command database.\n"
    "\tIf no sources are specified, all the files from the compilation database\n"
    "\twill be used.\n";
// clang-format on
//
} // namespace

tl::expected<tool::cli::options_old, std::string> tool::cli::parse_old(int argc,
  char **argv) noexcept {
  namespace cl = llvm::cl;
  cl::extrahelp common_help{help_message};
  cl::OptionCategory option_category{"Generation Options"};

  cl::opt<std::string> cl_compilation_db_path{
    cl::cat(option_category),
    "p",
    cl::desc("Specify path to `compile_commands.json`"),
    cl::value_desc("path"),
    cl::Required,
    cl::ValueRequired,
  };

  cl::opt<std::string> cl_output_file{
    cl::cat(option_category),
    "o",
    cl::desc("Specify output filename"),
    cl::value_desc("filename"),
    cl::Required,
    cl::ValueRequired,
  };

  cl::list<std::string> cl_source_paths{
    cl::cat(option_category),
    cl::desc("[<source>...]"),
    cl::ZeroOrMore,
    cl::Positional,
  };

  cl::list<std::string> cl_excluded_folders{
    cl::cat(option_category),
    "excluded",
    cl::cat(option_category),
    cl::desc("Specify paths within `compile_commands.json` to ignore"),
    cl::value_desc("path or filename"),
    cl::ZeroOrMore,
    cl::ValueOptional,
  };

  cl::opt<std::string> cl_resource_dir{
    cl::cat(option_category),
    "resource-dir",
    cl::desc("Directory for system clang headers"),
    cl::Hidden,
    // todo: consider adding default value
    cl::Required,
  };

  // todo: consider using verbosity level instead
  cl::opt<bool> cl_debug{
    cl::cat(option_category),
    // fixme: for some reason I can't have "debug"
    "print-debug",
    cl::desc("Print debug output"),
    cl::init(false),
    cl::Hidden,
    cl::ValueOptional,
  };

  cl::ResetAllOptionOccurrences();
  cl::HideUnrelatedOptions(option_category);
  std::string err_buffer;
  llvm::raw_string_ostream err_stream{err_buffer};
  if (!cl::ParseCommandLineOptions(argc,
        argv,
        /*Overview=*/{},
        &err_stream)) {
    return tl::unexpected(std::move(err_buffer));
  }

  // todo: validate
  std::filesystem::path resource_dir = cl_resource_dir.getValue();

  // todo: validate
  std::filesystem::path compilation_db_path = cl_compilation_db_path.getValue();

  // todo: validate
  std::filesystem::path output_file = cl_output_file.getValue();

  std::vector<std::filesystem::path> sources;
  if (auto paths = util::to_std_paths(cl_source_paths); !paths)
    return tl::unexpected(std::move(paths).error());
  else
    sources = std::move(paths).value();

  std::vector<std::filesystem::path> excluded_folders;
  if (auto paths = util::to_std_paths(cl_source_paths); !paths)
    return tl::unexpected(std::move(paths).error());
  else
    sources = std::move(paths).value();

  tl::expected<tool::cli::options_old, std::string> result{
    tool::cli::options_old{
      .resource_dir = std::move(resource_dir),
      .output_file = std::move(output_file),

      .compilation_db_path = std::move(compilation_db_path),
      .sources = std::move(sources),
      .excluded_folders = std::move(excluded_folders),

      .print_debug = cl_debug.getValue(),
    },
  };

  // todo: add option for resource-dir to just print it

  const auto to_string = [](const std::filesystem::path &p) {
    return p.string();
  };
  // todo: print conditionally (add flag parameter)
  fmt::println("--resource-dir={}", result->resource_dir.string());
  fmt::println("-p={}", result->compilation_db_path.string());
  fmt::println("-o={}", result->output_file.string());
  fmt::println("--excluded({})=[{}]",
    result->excluded_folders.size(),
    fmt::join(util::converted(to_string, result->excluded_folders), ", "));
  fmt::println("sources({}) [{}]",
    result->sources.size(),
    fmt::join(util::converted(to_string, result->sources), ", "));
  fmt::println("--print-debug={}", result->print_debug);

  const auto &_debug_result = *result;
  (void)_debug_result;
  return result;
}

std::string tool::cli::to_string(verbosity_level v) noexcept {
  // todo: (?) assert
  constexpr std::string_view s_debug = "debug";
  constexpr std::string_view s_input = "input";
  constexpr std::string_view s_parsed_types = "parsed_types";

  if (!v)
    return "none";

  std::vector<std::string_view> values;
  if (verbosity_level::debug & v)
    values.emplace_back(s_debug);

  if (verbosity_level::input & v)
    values.emplace_back(s_input);

  if (verbosity_level::parsed_types & v)
    values.emplace_back(s_parsed_types);

  return fmt::format("{}", fmt::join(values, "|"));
}

std::string tool::cli::to_string(const options &o) {
  std::string mode = std::visit(
    [](const auto &_m) -> std::string {
      using mode_type = std::decay_t<decltype(_m)>;
      if constexpr (std::is_same_v<target_mode, mode_type>) {
        const target_mode &m = _m;
        return fmt::format(
          R"(target-mode,
--output-file={},
--output-dir={})",
          m.output_file.string(),
          m.output_dir.string());
      } else {
        const inplace_mode &m = _m;
        return fmt::format(
          R"(inplace-mode,
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
        return fmt::format("--compilation-db={}", v.path.string());
        // todo:
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

tl::expected<tool::cli::options, std::pair<std::string, int>>
  tool::cli::parse(int argc, const char *const *argv) noexcept {
  CLI::App app{
    R"(
C++ reflection code generator that operates in two modes:

Target Mode (default):
  Generates a single .cpp file containing reflected call implementations for a 
  CMake target. The output will be linked to the target via a CMake function.

Inplace Mode:
  Generates .hpp header files containing reflected call implementations for 
  given .cpp files. Headers are implicitly included at the start of each 
  translation unit. Note: Uses friend injection, which is not guaranteed by 
  the C++ Standard.
)"};

  // tool [--inplace-mode] [--target=<target_name>] [--compilation-db=<dir>]
  // [sources...] [-o,--out-file=<file>] [--output-dir=<dir>]
  bool cli_in_place_mode = false;
  const auto &opt_inplace_mode =
    app.add_flag("--inplace-mode", cli_in_place_mode)
      ->description("Use Inplace Mode");

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
      .add_option("--compilation-db",
        cli_compilation_db_dir,
        "Path to 'compilie_commands.json' dir.")
      ->type_name("PATH")
      ->excludes(opt_cli_cl_flags);

  // todo: parse
  verbosity_level cli_verbosity = verbosity_level::none;

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
    ->excludes(opt_inplace_mode);

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
    return tl::unexpected(std::pair{std::move(ss).str(), code});
  }

  using mode_type = decltype(options::mode);
  using cl_flags_type = decltype(options::cl_flags);
  return options{
    .mode = cli_in_place_mode //
      ? mode_type{inplace_mode{
          .output_dir = std::move(cli_output_dir),
        }}
      : mode_type{target_mode{
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

tl::expected<tool::cli::options, std::string> tool::cli::evaluate_defaults(
  options o) noexcept {
  // todo: evaluate (resource dir, whatever...)
  if (!o.resource_dir.empty()) {
    // todo:
  }

  // .mode
  std::visit(
    [](auto &_m) {
      using mode_type = std::decay_t<decltype(_m)>;
      if constexpr (std::is_same_v<target_mode, mode_type>) {
        target_mode &m = _m;
        if (m.output_dir.empty())
          m.output_dir = std::filesystem::current_path();
        if (m.output_file.empty())
          m.output_file = "reflected.cpp";
      } else {
        inplace_mode &m = _m;
        m.output_dir = std::filesystem::current_path();
      }
    },
    o.mode);

  // todo: (? this might require complex logic) .resource_dir

  return o;
}
