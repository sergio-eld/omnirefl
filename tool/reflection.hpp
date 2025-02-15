#pragma once

#include "tool/ast.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <clang/AST/DeclTemplate.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Frontend/ASTUnit.h>
#pragma GCC diagnostic pop

#include <tl/expected.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

// iteration 2 implementation
namespace tool::refl {
struct context;

namespace matches {

struct reflected_type_index {
  // todo: remove. `binding_tag` is implementation-specific
  constexpr static const char binding_tag[] = "reflected_type_index";

  // todo: this should be deducible from the ASTMatchers' expression
  using node_type = clang::ClassTemplateSpecializationDecl;

  auto operator()() const noexcept {
    using namespace clang::ast_matchers;

    return classTemplateSpecializationDecl( //
      unless(isInStdNamespace()),
      hasAncestor(namespaceDecl(hasName("omni"))),
      hasAncestor(namespaceDecl(hasName("detail"))),
      hasName("_type_index"),
      isTemplateInstantiation(),
      isDefinition()
      //
    );
  }

  static tl::expected<context, std::string> resolve(const node_type &node,
    const clang::ASTUnit &ast,
    const context &ctx,
    bool print_debug = false);
};

// types reflected via instantiation of a designated template struct
struct reflected_type {
  // todo: remove. `binding_tag` is implementation-specific
  constexpr static const char binding_tag[] = "reflected_type";

  // todo: this should be deducible from the ASTMatchers' expression
  using node_type = clang::ClassTemplateSpecializationDecl;

  auto operator()() const noexcept {
    using namespace clang::ast_matchers;

    return classTemplateSpecializationDecl( //
      unless(isInStdNamespace()),
      hasAncestor(namespaceDecl(hasName("omni"))),
      hasAncestor(namespaceDecl(hasName("detail"))),
      hasName("_reflected_type"),
      isTemplateInstantiation(),
      isDefinition()
      //
    );
  }

  static tl::expected<context, std::string> resolve(const node_type &node,
    const clang::ASTUnit &ast,
    const context &ctx,
    bool print_debug = false);
};

// implementation types reflected via instantiation of a designated template struct
struct reflected_impl {
  // todo: remove. `binding_tag` is implementation-specific
  constexpr static const char binding_tag[] = "reflected_impl";

  // todo: this should be deducible from the ASTMatchers' expression
  using node_type = clang::ClassTemplateSpecializationDecl;

  auto operator()() const noexcept {
    using namespace clang::ast_matchers;

    return classTemplateSpecializationDecl( //
      unless(isInStdNamespace()),
      hasAncestor(namespaceDecl(hasName("omni"))),
      hasAncestor(namespaceDecl(hasName("detail"))),
      hasName("_reflected_impl"),
      isTemplateInstantiation(),
      isDefinition()
      //
    );
  }

  static tl::expected<context, std::string> resolve(const node_type &node,
    const clang::ASTUnit &ast,
    const context &ctx,
    bool print_debug = false);
};

// invocations
struct reflected_call {
  // todo: remove. `binding_tag` is implementation-specific
  constexpr static const char binding_tag[] = "matched_reflected_call";

  // todo: this should be deducible from the ASTMatchers' expression
  using node_type = clang::CXXMethodDecl;

  // REMEMBER: DONT USE IT!!!!
  // unless(isExpansionInSystemHeader()),
  auto operator()() const noexcept {
    using namespace clang::ast_matchers;
    return cxxMethodDecl( //
      unless(isInStdNamespace()),
      hasAncestor(namespaceDecl(hasName("omni"))),
      hasAncestor(cxxRecordDecl(hasName("reflected_call_t"))),
      isTemplateInstantiation(),
      hasName("_call_impl"));
  }

  static tl::expected<context, std::string> resolve(const node_type &node,
    const clang::ASTUnit &ast,
    const context &ctx,
    bool print_debug = false);
};

// debug
struct _debug_templ_spec_decl {
  static constexpr const char binding_tag[] = "asshole_tag";

  using node_type = clang::ClassTemplateSpecializationDecl;

  auto operator()() const noexcept {
    using namespace clang::ast_matchers;
    return classTemplateSpecializationDecl( //
      hasAncestor(namespaceDecl(hasName("omni"))),
      hasAncestor(namespaceDecl(hasName("detail"))),
      isTemplateInstantiation(),
      isDefinition()
      //
    );
  }

  static tl::expected<context, std::string> resolve(const node_type &node,
    const clang::ASTUnit &ast,
    const context &ctx,
    bool print_debug = false);
};

} // namespace matches

// flags for definition properties
// these are used to determine violated limitations when using the tool
enum type_definition_flags {
  none = 0x0,
  // unnamed structure
  unnamed = 0x1,
  // definition within a scope
  local = 0x1 << 1,
  // defined within a .cpp file (source): when using a standalone tool
  // definition can (probably) be generated within the same .cpp file via
  // a force-include compiler command
  in_cpp = 0x1 << 2,
};

enum reference_type {
  ref_none,
  ref_lval,
  ref_rval,
};

struct type_definition_data {
  std::filesystem::path source_file;
  type_definition_flags definition_flags = none;
  bool is_class;
};

struct struct_field_data {
  std::string name;

  // fully namespace-qualified type, used to uniquely identify the type
  std::string nm_qual_type;

  // todo: I don't know how this can be useful, since there's a simple workaround for accessing
  // bit fields without a member pointer
  // bool is_bitfield;
};

struct function_signature_arg {
  // fully namespace-qualified type, used to uniquely identify the type
  std::string nm_qual_type;
  bool is_const : 1;
  reference_type ref_type;
};

struct func_signature {
  std::vector<function_signature_arg> args;
};

// todo: can this type really be independent from the matches?
// structure that collects and saves intermediate state from several ASTs
struct context {
  // todo: profile and optimize
  // definitions for reflected types and implementations
  std::unordered_map<std::string /*namespace_qualified_type*/, type_definition_data> definitions;

  // todo: is it possible to have unresolved reflected types between ASTs? as of this writing - no,
  // because we assume that forward declarations are not allowed. however, if intermediate forward
  // declarations are allowed (defined in different AST), we should add a container to track them
  std::unordered_map<std::string /*namespace_qualified_type*/, std::vector<struct_field_data>>
    reflected_types;

  std::set<std::string /*namespace_qualified_type*/> reflected_implementations;
  std::vector<func_signature> reflected_calls;

  // todo: profile and optimize with flat_set
  // unique list of std headers. used for reflecting std containers and aggregated types like
  // `std::tuple`, `std::variant`, `std::optional` and any other standard type that wraps a
  // reflected type and defines `type` or `value_type` trait
  std::set<std::string> std_includes;

  // fixme: this should not be present when working with several .cpp files in non-inline-reflection
  // mode... as of now as an ad hoc I can store for each processed .cpp and erase before processing
  // the next .cpp
  std::unordered_map<int, std::string> reflected_types_indexes;
};

// refactorme: this function should be in ast.hpp. Now it depends only on the `context`
template <typename... Matches>
tl::expected<context, std::string> resolve_matched_node(
  const matched_node_variant<Matches...> &match,
  const clang::ASTUnit &ast,
  const context &ctx,
  bool print_debug = false) noexcept {
  return std::visit(
    [&]<typename Match>(const matched_node<Match> &m) { //
      return Match::resolve(*m.node, ast, ctx, print_debug);
    },
    match);
}

tl::expected<tool::refl::context, std::string>
  update(context current, context delta, bool print_debug) noexcept;
} // namespace tool::refl
