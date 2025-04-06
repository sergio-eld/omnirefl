#pragma once

#include "tool/cli.hpp"

#include <tl/expected.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace clang::tooling {
class CompilationDatabase;
} // namespace clang::tooling

namespace tool::header_mode {

// refactorme:
//   1. inverse dependencies with `cli`.
//      `run_pipeline` is the 'primary' interface function that defines its own
//      parameters. `cli` just exposes them, so `cli.h` should be the one
//      including this header
//
//   2. group options.
//      - config: (resource_dir). 
//      - input: sources, output dir, compilation_db
//
tl::expected<tl::monostate, std::string> run_pipeline(
  const cli::header_mode &mode,
  const cli::options &cli,
  // refactorme:
  //   one source + flags + 'cache'
  //   Pipeline invocation can return a set of (header) dependencies to be
  //   tracked by the build system (ninja, make, or even cmake), which would
  //   allow a fine grained control over regenerating reflection for only those
  //   sources that have changed
  const std::vector<std::filesystem::path> &sources,
  // todo: get rid of compilation_db
  const clang::tooling::CompilationDatabase &compilation_db);

} // namespace tool::header_mode
