#pragma once

// refactorme:
//   inverse dependencies. cli should include the header, because `run_pipeline`
//   is the one which defines which parameters it uses
#include "tool/cli.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace clang::tooling {
class CompilationDatabase;
} // namespace clang::tooling

namespace tool::source_mode {

std::expected<std::monostate, std::string> run_pipeline(
  const cli::source_mode &mode,
  const cli::options &cli,
  const std::vector<std::filesystem::path> &sources,
  const clang::tooling::CompilationDatabase &compilation_db) noexcept;

} // namespace tool::source_mode
