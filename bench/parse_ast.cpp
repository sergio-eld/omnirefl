
#include "tool/ast.hpp"

#include <benchmark/benchmark.h>

#include <fmt/format.h>

#include <filesystem>

#ifndef DB_PATH
#  error "DB_PATH not set"
#endif

#ifndef CPP_PATH
#  error "CPP_PATH not set"
#endif

#ifndef RESOURCE_DIR
#  error "RESOURCE_DIR not set"
#endif

namespace {
void bm_parse_ast(benchmark::State &state) {
  const std::filesystem::path db_path = {DB_PATH};
  auto compilation_db = tool::load_compilation_db(db_path);
  if (!compilation_db) {
    state.SkipWithError(fmt::format("Failed to load compilation DB: {}", db_path.string()).c_str());
    return;
  }

  const std::filesystem::path cpp_path = {CPP_PATH};
  if (compilation_db.value()->getCompileCommands(cpp_path.c_str()).empty()) {
    state.SkipWithError(
      fmt::format("Failed to find {} in compilation db {}", cpp_path.string(), db_path.string())
        .c_str());
    return;
  }

  const std::filesystem::path resource_dir = {RESOURCE_DIR};

  for (auto _ : state) {
    auto res = tool::parse_ast_from_source(resource_dir, cpp_path, **compilation_db);
    if (!res) {
      state.SkipWithError(fmt::format("Failed to parse AST of {}", cpp_path.string()).c_str());
      return;
    }
    state.PauseTiming();
    res.value().reset();
    state.ResumeTiming();
  }
}
} // namespace

// todo: args:
// - sources
BENCHMARK(bm_parse_ast);

BENCHMARK_MAIN();
