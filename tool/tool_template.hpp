#pragma once

#include "tool/data.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <clang/Tooling/CompilationDatabase.h>
#pragma GCC diagnostic pop

#include <memory>
#include <tl/expected.hpp>

namespace tool {
// default implementation for cli parsing
struct parse_cli_t {
  tl::expected<data::cli_opts, std::string> operator()(int argc, char **argv) const noexcept;
} const inline parse_cli{};

struct load_compilation_db_t {
  tl::expected<std::unique_ptr<clang::tooling::CompilationDatabase>, std::string> //
    operator()(const std::filesystem::path & compilation_db_path) const noexcept {
    std::string err;
    auto ptr =
      clang::tooling::CompilationDatabase::loadFromDirectory(compilation_db_path.string(), err);

    // todo: reference the path in the error
    if (!ptr)
      return tl::unexpected(std::move(err));
    return {
      tl::in_place,
      std::move(ptr),
    };
  };
} const inline load_compilation_db{};

struct filter_db_sources_t {
  struct args {
    std::vector<std::filesystem::path> specified_sources;
    std::vector<std::filesystem::path> excluded_folders;
  };

  tl::expected<std::vector<std::filesystem::path>, std::string> operator()(args a,
    std::vector<std::filesystem::path> db_sources) const noexcept;
} const inline filter_db_sources{};

} // namespace tool
