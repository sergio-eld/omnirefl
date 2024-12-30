#include "tool/tool_template.hpp"

#include "tool/util.hpp"

#include <fmt/base.h>
#include <fmt/ranges.h>

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Error.h>

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

tl::expected<data::cli_opts, std::string> tool::parse_cli_t::operator()(int argc,
  char **argv) const noexcept {
  namespace cl = llvm::cl;
  cl::extrahelp common_help{help_message};
  cl::OptionCategory option_category{"Generation Options"};

  cl::opt<std::string> cl_compilation_db_path(cl::cat(option_category),
    "p",
    cl::desc("Specify path to `compile_commands.json`"),
    cl::value_desc("path"),
    cl::Required,
    cl::ValueRequired);

  cl::opt<std::string> cl_output_file(cl::cat(option_category),
    "o",
    cl::desc("Specify output filename"),
    cl::value_desc("filename"),
    cl::Required,
    cl::ValueRequired);
  cl::list<std::string> cl_source_paths(cl::cat(option_category),
    cl::desc("[<source>...]"),
    cl::ZeroOrMore,
    cl::Positional);

  cl::list<std::string> cl_excluded_folders(cl::cat(option_category),
    "excluded",
    cl::desc("Specify paths within `compile_commands.json` to ignore"),
    cl::value_desc("path or filename"),
    cl::ZeroOrMore,
    cl::ValueOptional);

  cl::opt<std::string> cl_resource_dir{
    "resource-dir",
    cl::cat(option_category),
    cl::desc("Directory for system clang headers"),
    cl::Hidden,
    // todo: consider adding default value
    cl::Required,
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

  tl::expected<data::cli_opts, std::string> result{data::cli_opts{
    .resource_dir = std::move(resource_dir),
    .output_file = std::move(output_file),

    .compilation_db_path = std::move(cl_compilation_db_path),
    .sources = std::move(sources),
    .excluded_folders = std::move(excluded_folders),
  }};

  // todo: add option for resource-dir to just print it

  const auto to_string = [](const std::filesystem::path &p) { return p.string(); };
  // todo: print conditionally (add flag parameter)
  fmt::println("-resource-dir={}", result->resource_dir.string());
  fmt::println("-p=", result->compilation_db_path.string());
  fmt::println("-o=", result->output_file.string());
  fmt::println("-excluded=[{}]",
    fmt::join(util::converted(to_string, result->excluded_folders), ", "));
  fmt::println("sources [{}]", fmt::join(util::converted(to_string, result->sources), ", "));

  const auto &_debug_result = *result;
  (void)_debug_result;
  return result;
}
