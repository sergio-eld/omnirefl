#pragma once

#include "tool/cli.hpp"
#include "tool/util.hpp"

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

#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace tool {
// todo: add continious benchmarking to CI before optimizing this
std::expected<std::unique_ptr<clang::ASTUnit>, std::string>
  parse_ast_from_source(const cli::source_mode &,
    const std::filesystem::path &resource_dir,
    const std::filesystem::path &source,
    // refactorme: pass command-line args
    const clang::tooling::CompilationDatabase &db,
    // to disambiguate the input file if db contains several entries for
    // diffenet targets
    const std::optional<std::filesystem::path> &output_file,
    tool::cli::verbosity_level = tool::cli::verbosity_level::none) noexcept;

std::expected<std::unique_ptr<clang::ASTUnit>, std::string>
  parse_ast_from_source(const cli::header_mode &,
    const std::filesystem::path &resource_dir,
    const std::filesystem::path &source,
    // refactorme: pass command-line args
    const clang::tooling::CompilationDatabase &db,
    // to disambiguate the input file if db contains several entries for
    // diffenet targets
    const std::optional<std::filesystem::path> &output_file,
    tool::cli::verbosity_level = tool::cli::verbosity_level::none) noexcept;

std::expected<std::unique_ptr<clang::tooling::CompilationDatabase>, std::string>
  load_compilation_db(
    const std::filesystem::path &compilation_db_path) noexcept;

template <typename Match, typename ResolveNode, typename FoldResult>
struct node_transform {
  using match_node_type = Match;

  Match match_node;
  ResolveNode resolve_node;

  // todo: requires can be invoked with `Accum, Result`. However, Accum is a
  // template arg and is not known at this point
  FoldResult fold_result;

  // fixme:
  //   ResolveNode must be invocable with:
  //   (const node_type&, const clang::ASTUnit&, const Typename& resolved)
  //
  // static_assert(requires(const Match &m) {
  //   typename Match::node_type;
  //   { Match::binding_tag } -> std::convertible_to<std::string_view>;
  //   // todo: validate node_type from `m()` invocation
  //   m();
  // });

  // static_assert(requires(const ResolveNode &resolve,
  //   const typename Match::node_type &n,
  //   const clang::ASTUnit &ast) {
  //   std::expected{resolve(n, ast)};
  //   { resolve(n, ast).error() } -> std::convertible_to<std::string>;
  // });
};

template <typename T, template <typename...> class Template>
struct is_of_template: std::false_type {};

template <template <typename...> class Template, typename... T>
struct is_of_template<Template<T...>, Template>: std::true_type {};

template <typename T, template <typename...> class Template>
concept of_template = requires { is_of_template<T, Template>{}; };

/*
 * refactorme: core pipeline should be as simple as:
 *
 * [(source, [flags], [opts])]
 * | compiler_invocation -> ast
 * | fold(context, matchers_by_mode[mode]) -> context
 * | generator_by_mode[mode]
 *
 */

// refactorme: modular design. it should be easy to do in-place composition
// instead of forcing 'complex logic' upon a user
template <typename Mode, typename... NodeTransforms>
struct tu_pipeline {
  static_assert((of_template<NodeTransforms, node_transform> && ...));
  const std::filesystem::path &resource_dir;

  // todo: remove this atrocity
  const clang::tooling::CompilationDatabase &compilation_db;
  const cli::verbosity_level verbosity;

  // refactorme:
  //   this doesn't seem to be necessary to
  //   'run transforms on a translation unit'.
  //   It is more a side effect of the leaking (through encapsulation) clang
  //   tooling
  // todo: remind meself why I need it here...
  const Mode &mode;

  std::tuple<NodeTransforms...> transforms;

  // refactorme:
  //   I could have ASTUnit as an input instead of `src`
  //   Also, why would I necessarilly need `Accum` here?
  template <typename Accum>
  std::expected<Accum, std::string> operator()(Accum _accum,
    const std::filesystem::path &src) const noexcept {
    std::expected ast = tool::parse_ast_from_source(mode,
      resource_dir,
      src,
      // todo: continious benchmark before
      // todo: remove dependency on compilation_db, pass flags direclty
      compilation_db,
      /*output_file=*/std::nullopt, //< fixme: pass the file
      verbosity);
    if (!ast) {
      return std::unexpected(fmt::format("Error parsing AST of file {}: {}",
        src.string(),
        std::move(ast).error()));
    }

    using fetch_transform = std::variant<const NodeTransforms *...> (*)(
      const std::tuple<NodeTransforms...> &);

    static const std::map k_transform_by_tag_map = std::invoke(
      []<size_t... I>(std::index_sequence<I...>,
        std::type_identity<NodeTransforms>...)
        -> std::map<std::string_view, fetch_transform> {
        return {{std::string_view{NodeTransforms::match_node_type::binding_tag},
          static_cast<fetch_transform>(
            [](const std::tuple<NodeTransforms...> &t)
              -> std::variant<const NodeTransforms *...> {
              return std::addressof(std::get<I>(t));
            })}...};
      },
      std::index_sequence_for<NodeTransforms...>(),
      std::type_identity<NodeTransforms>{}...);

    std::expected<Accum, std::string> accum{std::move(_accum)};
    // adapter
    using namespace clang::ast_matchers;
    struct: MatchFinder::MatchCallback {
      const tu_pipeline *_this;
      const clang::ASTUnit *ast;
      Accum *accum;
      std::vector<std::string> errors;

      // todo: fail on error/recovery options
      bool fatal_error = false;

      void run(const MatchFinder::MatchResult &mresult) override {
        for (const auto &[str_tag, node] : mresult.Nodes.getMap()) {
          const auto n_transform =
            k_transform_by_tag_map.find(std::string_view(str_tag));
          if (k_transform_by_tag_map.cend() == n_transform) {
            errors.emplace_back(fmt::format("unmatched tag {}", str_tag));
            continue;
          }

          if (cli::verbosity_level::parsed_types & _this->verbosity)
            fmt::println("matched tag {}", str_tag);

          if (std::expected resolved = std::visit(
                [&]<typename Match, typename ResolveNode, typename FoldResult>(
                  const node_transform<Match, ResolveNode, FoldResult>
                    *_node_transform)
                  -> std::expected<std::monostate, std::string> {
                  const node_transform<Match, ResolveNode, FoldResult>
                    &node_transform = *_node_transform;
                  const auto *expected_node =
                    node.template get<typename Match::node_type>();
                  if (!expected_node) {
                    return std::unexpected(
                      fmt::format("Error: unexpected node type for tag {}",
                        str_tag));
                  }

                  std::expected resolved_reflection_data =
                    node_transform.resolve_node(*expected_node, *ast, *accum);
                  if (!resolved_reflection_data) {
                    return std::unexpected(fmt::format(
                      "Error: failed resolving reflection data for tag {}: {}",
                      str_tag,
                      std::move(resolved_reflection_data).error()));
                  }

                  std::expected updated_accum =
                    node_transform.fold_result(std::move(*accum),
                      std::move(resolved_reflection_data).value());
                  if (!updated_accum) {
                    fatal_error = true;
                    return std::unexpected(fmt::format(
                      "Error: failed updating reflection data for tag {}: {}",
                      str_tag,
                      std::move(updated_accum).error()));
                  }

                  *accum = std::move(updated_accum).value();

                  return {};
                },
                n_transform->second(_this->transforms));
            !resolved) {
            errors.emplace_back(std::move(resolved).error());
            if (fatal_error)
              return;
          }
        }
      }
    } match_callback;
    // Can't call designated initialization of inheriting class
    match_callback._this = this;
    match_callback.ast = &**ast;
    match_callback.accum = std::addressof(accum.value());

    MatchFinder finder;
    std::apply(
      // refactorme:
      //   repeating template args here is ugly and error prone. As of now I
      //   don't see an alternative if I want to use `node_transform` as a
      //   variadic lambda argument
      [&finder, &match_callback]<typename... Match,
        typename... Resolve,
        typename... Fold>(const node_transform<Match, Resolve, Fold> &...) {
        (finder.addMatcher(
           // todo: TraverseKind from MatchNode ?
           // TK_AsIs is needed to include template instantiations
           traverse(clang::TK_AsIs, Match{}().bind(Match::binding_tag)),
           &match_callback),
          ...);
      },
      transforms);

    finder.matchAST((*ast)->getASTContext());
    if (!match_callback.errors.empty()
      && cli::verbosity_level::info & verbosity) {
      fmt::println("Errors while running translation unit pipeline:\n{}",
        util::join(match_callback.errors, "\n", [] { return "{}"; }));
    }

    if (!match_callback.fatal_error)
      return accum;

    return std::unexpected(
      fmt::format("{}", fmt::join(std::move(match_callback.errors), "\n")));
  }
};

} // namespace tool
