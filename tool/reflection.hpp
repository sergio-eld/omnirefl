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
#include <unordered_set>
#include <vector>

// iteration 2 implementation
namespace tool::refl {

// flags for definition properties
// these are used to determine violated limitations when using the tool
enum type_definition_flags {
  none = 0x0,
  // todo: this flag is redundant, if I have a namespace-qualified typename
  // unnamed structure
  unnamed = 0x1,
  // definition within a scope
  local = 0x1 << 1,
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
  // todo: add new data when needed. As of now, only field names are needed for
  // current implementation. Let's not overcomplicate
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

// todo: remove
struct context;

namespace matches {

struct reflected_type_data {
  type_definition_data definition_data;
  std::vector<struct_field_data> fields;
};

// todo:
//   do I need to add a specialization based on `mode`? For inplace mode headers
//   are not needed
//
// types reflected via instantiation of a designated template struct
struct reflected_type {
  // todo: remove. `binding_tag` is implementation-specific
  constexpr static const char binding_tag[] = "reflected_type";

  // todo: this should be deducible from the ASTMatchers' expression
  using node_type = clang::ClassTemplateSpecializationDecl;
  auto operator()() const noexcept {
    using namespace clang::ast_matchers;

    // matching `omni::detail::_reflected_type<T>`
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

  // fixme: what about fundamental or std types?
  struct result {
    // nullopt, if data has been already reflected, or if it is a
    // non-reflectable type: for example, std::tuple<T...> itself can't be
    // reflected, but types `T...` can.
    std::optional<std::pair<std::string /*nm_qual_type*/, reflected_type_data>>
      matched_type;
    std::unordered_map<std::string, reflected_type_data>
      matched_type_dependencies;
  };

  static tl::expected<result, std::string> resolve(const node_type &node,
    const clang::ASTUnit &ast,
    const std::unordered_set<std::string> &resolved_types,
    bool print_debug = false);
};

// only needed for inplace mode
struct reflected_indexed_type {
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

  struct result {
    std::pair<size_t /*type_index*/, reflected_type_data> matched_type;
    std::optional<std::string> nm_qual_type;

    std::unordered_map<std::string, reflected_type_data>
      matched_type_dependencies;
    // todo:
    //   maybe add support for private types, but it would require using
    //   includes
  };

  static tl::expected<result, std::string> resolve(const node_type &node,
    const clang::ASTUnit &ast,
    const std::unordered_set<std::string> &resolved_types,
    bool print_debug = false);
};

// todo:
//   consider removing this separate tag and just use `reflected_call`. having
//   those as separate introduces possible inconsistency: reflected_call without
//   reflected_impl doesn't make any sense
//
// implementation types reflected via instantiation of a designated template
// struct
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

  struct result {
    // namespace-qualified type of the callable type
    std::string callable_nm_qual_type;
    type_definition_data callable_definition_data;
  };

  static tl::expected<result, std::string> resolve(const node_type &node,
    const clang::ASTUnit &ast,
    const std::unordered_set<std::string> &resolved_types,
    bool print_debug = false);
};

// invocations
// only needed for target mode
struct reflected_call {
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

  struct result {
    func_signature call_signature;
    std::set<std::filesystem::path> std_includes;
    std::set<std::filesystem::path> includes;
  };

  static tl::expected<result, std::string> resolve(const node_type &node,
    const clang::ASTUnit &ast,
    const std::unordered_set<std::string> &resolved_types,
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

  struct result {};
  // todo: can I avoid having the same signature?
  static tl::expected<result, std::string> resolve(const node_type &node,
    const clang::ASTUnit &ast,
    const std::unordered_set<std::string> &resolved_types,
    bool print_debug = false);
};

} // namespace matches

// refactorme:
//   remove the 'blob' context class.
//   For inplace mode we only need to generate the types within a single
//   translation unit, so they must be isolated.
//   For target mode, the generated reflection symbols will be shared across
//   multiple translation units, so they must be shared... More over, in target
//   mode only non-local types that are defined in headers are allowed.
//
// todo: can this type really be independent from the matches?
// structure that collects and saves intermediate state from several ASTs
//
struct context {
  // todo: profile and optimize
  // definitions for reflected types and implementations
  std::unordered_map<std::string /*namespace_qualified_type*/,
    type_definition_data>
    definitions = {};

  // todo: is it possible to have unresolved reflected types between ASTs? as of
  // this writing - no, because we assume that forward declarations are not
  // allowed. however, if intermediate forward declarations are allowed (defined
  // in different AST), we should add a container to track them
  std::unordered_map<std::string /*namespace_qualified_type*/,
    std::vector<struct_field_data>>
    reflected_types = {};

  std::unordered_map<size_t, std::string /*namespace-qualified typename*/>
    detected_indexed_types = {};

  // as of this writing, for indexed types only struct fields matter
  std::unordered_map<size_t /*type_index*/, std::vector<struct_field_data>>
    reflected_indexed_types = {};

  std::set<std::string /*namespace_qualified_type*/> reflected_implementations =
    {};
  std::vector<func_signature> reflected_calls = {};

  // todo: profile and optimize with flat_set
  // unique list of std headers. used for reflecting std containers and
  // aggregated types like `std::tuple`, `std::variant`, `std::optional` and any
  // other standard type that wraps a reflected type and defines `type` or
  // `value_type` trait
  std::set<std::string> std_includes = {};
};

tl::expected<tool::refl::context, std::string>
  update(context current, context delta, bool print_debug) noexcept;
} // namespace tool::refl
