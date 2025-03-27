#pragma once

#include "tool/cli.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchersInternal.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/CompilationDatabase.h>
#pragma GCC diagnostic pop

#include <fmt/core.h>
#include <tl/expected.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace tool {
// todo: add continious benchmarking to CI before optimizing this
tl::expected<std::unique_ptr<clang::ASTUnit>, std::string>
  parse_ast_from_source(const cli::target_mode &,
    const std::filesystem::path &resource_dir,
    const std::filesystem::path &source,
    // refactorme: pass command-line args
    const clang::tooling::CompilationDatabase &db,
    tool::cli::verbosity_level = tool::cli::verbosity_level::none) noexcept;

tl::expected<std::unique_ptr<clang::ASTUnit>, std::string>
  parse_ast_from_source(const cli::inplace_mode &,
    const std::filesystem::path &resource_dir,
    const std::filesystem::path &source,
    // refactorme: pass command-line args
    const clang::tooling::CompilationDatabase &db,
    tool::cli::verbosity_level = tool::cli::verbosity_level::none) noexcept;

tl::expected<std::unique_ptr<clang::tooling::CompilationDatabase>, std::string>
  load_compilation_db(
    const std::filesystem::path &compilation_db_path) noexcept;

/**
 * // todo: refine Match expected traits
 * // should be something like mapping: match_decl -> (matched_node, context){}
 * struct Matcher {
 * // todo: remove/hide. This is some internal thing actually
 * constexpr static const char binding_tag[] = "tag";
 *
 * // todo: this should be deducible from the return type
 * using node_type = clang::ASTNodeT;
 *
 * auto operator()() const { return matchDecl(); }
 * };
 */
template <typename Match>
struct matched_node {
  using node_type = typename Match::node_type;
  const node_type *node;
};

// refactorme: this is _very_ confusing to use in combination with the variant
// wrapper refactorme: this should not be used for function signatures... User
// should only be responsible for providing an overload for his Match. That
// overload must otherwise adhere to expected signature
template <typename... Matches>
using matched_node_variant = std::variant<tool::matched_node<Matches>...>;

// todo: consider returning `vector<expected<node, error>>` instead
template <typename... Matches>
tl::expected<std::vector<matched_node_variant<Matches...>>, std::string>
  match_ast_nodes(
    // TBH it is used for type deduction
    std::vector<matched_node_variant<Matches...>> initial,
    // fixme: MatchFinder::matchAST expects a non-const ASTContext
    clang::ASTUnit &ast,
    // todo: verbosity
    bool print_debug = false) noexcept {
  using namespace clang::ast_matchers;

  // todo: instantiate binding tag strings here (somehow)

  // adapter
  struct: MatchFinder::MatchCallback {
    std::vector<matched_node_variant<Matches...>> result;
    std::optional<std::string> error = std::nullopt;
    // todo: verbosity
    bool print_debug;

    void run(const MatchFinder::MatchResult &mresult) override {
      if (print_debug)
        fmt::println("debug: matched {} nodes", mresult.Nodes.getMap().size());
      // todo: handle errors
      //  - tag has been found but type mismatches (it shouldn't be possible
      //  though)
      const auto add_node = [this](auto n, std::string_view tag) {
        if (n.node)
          result.push_back(n);
        else if (print_debug)
          fmt::println("debug: !!! unmatched tag {}", tag);
      };
      (add_node(
         matched_node<Matches>{
           .node = mresult.Nodes.getNodeAs<typename Matches::node_type>(
             Matches::binding_tag),
         },
         Matches::binding_tag),
        ...);
    }
  } match_callback;
  match_callback.result = std::move(initial);
  match_callback.print_debug = print_debug;

  MatchFinder finder;
  (finder.addMatcher(
     // todo: TraverseKind from MatchNode ?
     // TK_AsIs is needed to include template instantiations
     traverse(clang::TK_AsIs, Matches{}().bind(Matches::binding_tag)),
     &match_callback),
    ...);

  finder.matchAST(ast.getASTContext());
  if (match_callback.error)
    return tl::unexpected(std::move(match_callback.error).value());
  return std::move(match_callback.result);
}

} // namespace tool
