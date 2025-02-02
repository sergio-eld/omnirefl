
// todo:
// - ast caching (?)

// todo:
// #include "tool/..." // - tool related code
// #include "refl/..." // - reflection related code
// #include "plugin/..." // - plugin related code

#include "tool/emit_code.hpp"
#include "tool/reflection.hpp"
#include "tool/tool_template.hpp"
#include "tool/util.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated"
#include <clang/AST/ASTDumper.h>
#include <clang/Basic/SourceManager.h>
#pragma GCC diagnostic pop

#include <fmt/format.h>
#include <iostream>
#include <tl/expected.hpp>

#include <fstream>
#include <vector>

// allowing to configure specific things:
// - matchers
// - output to file
// - what else
int main(int argc, char **argv) {
  // todo: reduce error-handling boilerplate by using chained calls
  auto cli = tool::parse_cli(argc, argv);
  if (!cli) {
    llvm::errs() << cli.error();
    return -1;
  }

  auto compilation_db = tool::load_compilation_db(cli->compilation_db_path);
  if (!compilation_db) {
    llvm::errs() << std::move(compilation_db).error();
    llvm::errs() << cli->compilation_db_path.string() << '\n';
    return -1;
  }

  auto db_sources = util::to_std_paths((*compilation_db)->getAllFiles());
  if (!db_sources) {
    llvm::errs() << std::move(db_sources).error();
    llvm::errs() << cli->compilation_db_path.string() << '\n';
    return -1;
  }

  const auto filtered_sources = tool::filter_db_sources(
    {
      .specified_sources = cli->sources,
      .excluded_folders = cli->excluded_folders,
    },
    *db_sources);
  if (!filtered_sources) {
    llvm::errs() << std::move(filtered_sources).error();
    llvm::errs() << cli->compilation_db_path.string() << '\n';
    return -1;
  }

  // llvm doesn't know how to work with std::filesystem::path (so is fmt)
  // What a rotten way to die...
  const auto str_sources =
    [](const std::vector<std::filesystem::path> &paths) -> std::vector<std::string> {
    std::vector<std::string> res;
    res.reserve(paths.size());
    for (const auto &p : paths)
      res.push_back(p.string());
    return res;
  }(*filtered_sources);
  // todo: use verbosity flag
  fmt::println("Filtered sources: [{}]", fmt::join(str_sources, ", "));

  // todo: fold
  tool::refl::context ctx{};
  size_t processing = 0;
  // todo: util::indexed(*filtered_sources)
  for (const auto &src : *filtered_sources) {
    // todo: use verbosity flag
    fmt::println("[{}/{}] building AST of file: {}\t\r",
      ++processing,
      filtered_sources->size(),
      src.string());

    auto ast = tool::parse_ast({
      .resource_dir = cli->resource_dir,
      .source = src,
      .db = **compilation_db,
      .print_debug = cli->print_debug,
    });
    if (!ast) {
      llvm::errs() << ast.error();
      return -1;
    }

    auto matches = tool::match_ast(
      {
        .print_debug = cli->print_debug,
      },
      std::vector<tool::refl::variant_reflected_match>{},
      **ast);
    if (!matches) {
      llvm::errs() << matches.error();
      return -1;
    }

    // refactorme: these 2 calls (resolve && update) don't look clean
    // should `update` be implicit/part of the signature?
    auto ctx_delta = util::foldl(
      [&ast = **ast, &cli](tl::expected<tool::refl::context, std::string> ctx, const auto &match) {
      // refactorme: this will continue iterating on error...
        if (ctx) {
          return tool::refl::resolve_matched_node(
            {
              .print_debug = cli->print_debug,
            },
            ast,
            std::move(ctx).value(),
            match);
        }
        return ctx;
      },
      tl::expected<tool::refl::context, std::string>{tl::in_place},
      std::move(matches).value());

    if (!ctx_delta) {
      llvm::errs() << ctx_delta.error();
      return -1;
    }

    if (auto updated = update(cli->print_debug, std::move(ctx), std::move(ctx_delta).value()); updated) {
      ctx = std::move(updated).value();
    } else {
      llvm::errs() << updated.error();
      return -1;
    }
  }

  auto validated_reflection_data = codegen::prepare_input(std::move(ctx));
  if (!validated_reflection_data) {
    llvm::errs() << validated_reflection_data.error();
    return -1;
  }

  // todo: use verbosity flag
  fmt::println("Generating file: {}\n", cli->output_file.string());
  std::ofstream f{cli->output_file, std::ios::binary};
  if (const auto res = codegen::emit_code(
        {
          // todo: options
        },
        f,
        *validated_reflection_data);
      !res) {
    llvm::errs() << res.error() << '\n';
    return -1;
  };
  return 0;
}
