#pragma once

#include <tl/expected.hpp>

#include <filesystem>
#include <string>
#include <variant>
#include <vector>

namespace tool::cli {
/*
todo: consolidate cli options for different modes

currently, there are 2 modes (in progress):
- target_mode:

  for given 'cmake target' generates a .cpp file contaning implementations of
reflected calls, including necessary reflection types. Compiled translation unit
will be linked to the provided cmake target via a defined cmake function.

- inplace_mode:

  for a given .cpp file generates a .hpp header file containing implementations
of reflected calls, including the necessary reflection types. This header will
be implicitly included as the first header for the translation unit. Warning:
this mode relies on friend injection technique, validity of which is not
guaranteed by the C++ Standard.

cli:
- all modes:
  - compilation_db_path: '-p' path to compilation database, default value:
workdir
  - resource_dir: '--resource-dir' path to the tool's bundled headers (todo:
should be defaultable)

- target mode (default): '--target-mode'
  - output_file: '-o' name of the .cpp file containing generated implementations
of reflected calls (defaults to reflected_<target_name>.cpp)
  - output_dir: (todo) (defaults to workdir)
  - either:
    - source_paths: list of C++ sources to generate the reflection for
(currently tested/support guaranteed for .cpp files)
    - excluded_paths: list of C++ sources or folders to exclude. All the other
sources of the cmake target will be reflected

- inplace mode: '--inplace-mode'
  - output_dir: (todo) name of the folder to write the generated headers.
  - source_paths: list of .cpp files to generate reflection for

the tool should be invoked with workdir == CMAKE_BINARY_DIR
 */

// '=' in `<arg>=<value>` designates default value, [p] - positional arg
// ./tool [p]--exclude=false [p][<source>...] [p]-o=reflected_<target_name>.cpp
//   [p]--output_dir=<workdir> --compilation-db=<workdir>
//   --resource-dir=<deduced>
struct target_mode {
  std::filesystem::path output_file;
  std::filesystem::path output_dir;
};

struct inplace_mode {
  std::filesystem::path output_dir;
};

struct compilation_db_entry {
  std::filesystem::path path;
};

struct compilation_target {
  std::string name;
};

enum verbosity_level {
  none,
  debug = 1 << 0,
  input = 1 << 1,
  parsed_types = 1 << 2,
};

// todo: from_string
std::string to_string(verbosity_level v) noexcept;

struct options {
  std::variant<target_mode, inplace_mode> mode;

  // list of .cpp paths
  std::vector<std::filesystem::path> sources;

  // if .sources not empty, either list of (clang-compatible) compilation flags,
  // or a path to compile_commands.json, otherwise a path to
  // compile_commands.json (every .cpp file will be used as input)
  std::variant<std::vector<std::string>, compilation_db_entry> cl_flags;

  /// directory with bundled system headers
  std::filesystem::path resource_dir;

  verbosity_level verbosity = verbosity_level::none;
};

std::string to_string(const options &options);

struct options_old {
  /// directory for clang's system headers (bundled)
  std::filesystem::path resource_dir;

  /// path to where generate the reflected implementation
  std::filesystem::path output_file;

  /// path to compilation database (currenly only compile_commands.json)
  std::filesystem::path compilation_db_path;

  /// .cpp files to invoke the tool for.
  /// must be found within the compilation_db
  std::vector<std::filesystem::path> sources;

  std::vector<std::filesystem::path> excluded_folders;

  bool print_debug;
};

tl::expected<options_old, std::string> parse_old(int argc,
  char **argv) noexcept;

tl::expected<options, std::pair<std::string, int>> parse(int argc,
  const char *const *argv) noexcept;

// todo: do I need this as a standalone function? As of this writing it is used
// to remove observable side effects from `parse` (like resource_dir and workdir
// evaluation) for testability
tl::expected<options, std::string> evaluate_defaults(options o) noexcept;

// todo:
} // namespace tool::cli
