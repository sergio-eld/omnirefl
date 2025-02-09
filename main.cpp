
// todo:
// - ast caching (?)

// todo:
// #include "tool/..." // - tool related code
// #include "refl/..." // - reflection related code
// #include "plugin/..." // - plugin related code

#include "fmt/ranges.h"
#include "tool/ast.hpp"
#include "tool/emit_code.hpp"
#include "tool/reflection.hpp"
#include "tool/util.hpp"
#include "llvm/Support/raw_ostream.h"

#include <fmt/format.h>
#include <tl/expected.hpp>

#include <fstream>
#include <iostream>
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
      // todo: std::variant
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

  /*
   * // refactorme:
   * either<context, error> ctx_delta =
   *   std::tie(src, n_processing) >>=
   *     | unpack_to(parse_ast)
   *     | match_ast
   *     | foldl(std::move(ctx), resolve_matched_node);
   */
  const auto parse_ast = [&](const std::filesystem::path &src, size_t n) {
    // todo: use verbosity flag
    fmt::println( //
      "[{}/{}] building AST of file: {}\t\r",
      n,
      filtered_sources->size(),
      src.string());

    return tool::parse_ast_from_source( //
      cli->resource_dir,
      src,
      **compilation_db,
      cli->print_debug);
  };

  tool::refl::context ctx{};
  std::vector<std::string> errors;

  for (const auto &[src, n_processing] : util::indexed(*filtered_sources)) {
    tl::expected ast = parse_ast(src, n_processing + 1);
    if (!ast) {
      llvm::errs() << ast.error();
      return -1;
    }

    // refactorme: should it be a 'table' {match{}, resolve{}}?
    // this would allow to untie the match type from context type and provide different
    // variants of implementations
    using ast_match = tool::matched_node_variant< //
      tool::refl::matches::reflected_type,
      tool::refl::matches::reflected_impl,
      tool::refl::matches::reflected_call,
      tool::refl::matches::_debug_templ_spec_decl>;
    tl::expected matches = tool::match_ast_nodes( //
      std::vector<ast_match>{},
      **ast,
      cli->print_debug);
    if (!matches) {
      llvm::errs() << matches.error();
      return -1;
    }

    for (const ast_match &m : *matches) {
      tl::expected ctx_delta = //
        tool::refl::resolve_matched_node(m, **ast, ctx, cli->print_debug);
      if (!ctx_delta) {
        errors.emplace_back(std::move(ctx_delta).error());
        continue;
      }
      tl::expected updated = tool::refl::update(std::move(ctx), //
        std::move(ctx_delta).value(),
        cli->print_debug);
      if (!updated) {
        llvm::errs() //
          << fmt::format("Error updating context: {}", std::move(updated).error());
        return -1;
      }
      ctx = std::move(updated).value();
    }

    // todo: errors, add flag to stop on errors
    if (!errors.empty()) {
      llvm::errs() //
        << fmt::format("Errors while matching nodes: {}", fmt::join(errors, ", "));
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
