#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/CompilationDatabase.h>
#pragma GCC diagnostic pop

#include <fmt/base.h>
#include <tl/expected.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// todo: separate generic and implementation-specific code
namespace tool {
struct cli_opts {
  /// directory for clang's system headers (bundled)
  std::filesystem::path resource_dir;

  /// path to where generate the reflected implementation
  std::filesystem::path output_file;

  // todo: group options specific to invocation with compilation db.
  // one may want to invoke the tool on a single source file with compilation args

  /// path to compilation database (currenly only compile_commands.json)
  std::filesystem::path compilation_db_path;

  // todo: can they be optional?
  /// .cpp files to invoke the tool for.
  /// must be found within the compilation_db
  std::vector<std::filesystem::path> sources;

  std::vector<std::filesystem::path> excluded_folders;

  // todo: flags
  // - debug (duh)
  // - print parsed types
  // - print input args
  bool print_debug;
};

// default implementation for cli parsing
// generic code
struct parse_cli_t {
  tl::expected<cli_opts, std::string> operator()(int argc, char **argv) const noexcept;
} const inline parse_cli{};

// generic code
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

// generic code
struct filter_db_sources_t {
  struct args {
    // todo: these should be mutually-exclusive
    std::vector<std::filesystem::path> specified_sources;
    std::vector<std::filesystem::path> excluded_folders;
  };

  tl::expected<std::vector<std::filesystem::path>, std::string> operator()(args a,
    std::vector<std::filesystem::path> db_sources) const noexcept;
} const inline filter_db_sources{};

// todo: add continious benchmarking to CI before optimizing this
// generic code
struct parse_ast_t {
  struct args {
    const std::filesystem::path &resource_dir;

    // path to .cpp source file
    const std::filesystem::path &source;

    // todo: just compilation args
    const clang::tooling::CompilationDatabase &db;

    bool print_debug;
  };

  using result_t = tl::expected<std::unique_ptr<clang::ASTUnit>, std::string>;
  result_t operator()(args a) const;
} const inline parse_ast{};

// generic code
template <typename MatchT>
struct matched_node {
  using node_type = typename MatchT::node_type;
  const node_type *node;
};

// generic code
template <typename... T>
using match_variant = std::variant<tool::matched_node<T>...>;

// generic code
struct match_ast_t {
  struct args {
    bool print_debug;
  };
  // todo: refine MatchNode expected traits
  /**
   * struct MatchNode {
   * // todo: remove/hide. This is some internal thing actually
   * constexpr static const char binding_tag[] = "tag";
   *
   * // todo: this should be deducible from the return type
   * using node_type = clang::ASTNodeT;
   *
   * auto operator()() const { return matchDecl(); }
   * };
   */
  template <typename... MatchNode>
  tl::expected<std::vector<match_variant<MatchNode...>>, std::string> operator()(const args &a,
    std::vector<match_variant<MatchNode...>> initial,
    // MatchFinder::matchAST expects a non-const ASTContext
    clang::ASTUnit &ast) const noexcept {
    using namespace clang::ast_matchers;

    // adapter
    struct: MatchFinder::MatchCallback {
      std::vector<match_variant<MatchNode...>> result;
      std::optional<std::string> error = std::nullopt;
      bool print_debug;

      void run(const MatchFinder::MatchResult &mresult) override {
        if (print_debug)
          fmt::println("debug: matched {} nodes", mresult.Nodes.getMap().size());
        // todo: handle errors
        //  - tag has been found but type mismatches (it shouldn't be possible though)
        const auto add_node = [this](auto n, std::string_view tag) {
          if (n.node)
            result.push_back(n);
          else if (print_debug)
            fmt::println("debug: !!! unmatched tag {}", tag);
        };
        (add_node(
           matched_node<MatchNode>{
             .node = mresult.Nodes.getNodeAs<typename MatchNode::node_type>(MatchNode::binding_tag),
           },
           MatchNode::binding_tag),
          ...);
      }
    } match_callback;
    match_callback.result = std::move(initial);
    match_callback.print_debug = a.print_debug;

    MatchFinder finder;
    (finder.addMatcher(
       // TK_AsIs is needed to include template instantiations
       traverse(clang::TK_AsIs, MatchNode{}().bind(MatchNode::binding_tag)),
       &match_callback),
      ...);

    finder.matchAST(ast.getASTContext());
    if (match_callback.error)
      return tl::unexpected(std::move(match_callback.error).value());
    return std::move(match_callback.result);
  }
} constexpr inline match_ast{};
} // namespace tool
