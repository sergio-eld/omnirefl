#pragma once

#include "tool/cli.hpp"
#include "tool/reflection.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <clang/AST/DeclTemplate.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Frontend/ASTUnit.h>
#pragma GCC diagnostic pop

#include <optional>
#include <set>
#include <variant>
#include <vector>

namespace tool::header_mode {
namespace match {

struct reflected_type_data {
  refl::type_definition_data definition;
  std::vector<refl::struct_field_data> fields;
};

/**
 * Type, reflected as dependency of an indexed type, reflected via
 * `reflected_call`.
 * Usually will not have and index, if itself was not used as
 * a `reflected_call` argument.
 *
 * For types, reflected as dependency, there are practically (?) no limitations,
 * since we have full control of gerating and calling a proper specialization
 * even without a forward declaration.
 * For instance, indexed types are selected via indexed specialization for
 * reflection. Dependent types can be specialized via
 * `reflected_t<decltype(IndexedType::field)>`
 */
struct type_dependency {
  refl::type_id id;
  reflected_type_data reflected_data;

  friend bool operator<(const type_dependency &lhs,
    const type_dependency &rhs) noexcept {
    return lhs.id < rhs.id;
  }
};

struct reflectable {
  refl::type_id id;

  // unique index (generated once) for the reflected type within a translation
  // unit
  size_t index;
  reflected_type_data reflected_data;

  // refactorme: do I need set? Why not just use `std::map<type_id, reflected_type_data>`
  std::set<type_dependency, std::less<>> type_dependencies;
};

struct already_reflected {
  refl::type_id id;
};

/**
 * Type that itself can't be reflected, but has reflectable type dependencies:
 *  - fundamental types
 *  - std types or 3rd party, but typically will have dependent types
 *  - ???
 *
 * as of this writing, similarity to `reflectable` is incidental
 */
struct non_reflectable {
  refl::type_id id;

  size_t index;
  std::optional<refl::type_definition_data> definition;

  // refactorme: do I need the set?
  std::set<type_dependency, std::less<>> type_dependencies;
};

struct reflected_indexed_type {
  constexpr static const char binding_tag[] = "reflected_indexed_type";

  // todo: should this be deducible from the ASTMatchers' expression?
  using node_type = clang::ClassTemplateSpecializationDecl;
  auto operator()() const noexcept {
    using namespace clang::ast_matchers;

    return classTemplateSpecializationDecl( //
      unless(isInStdNamespace()),
      hasAncestor(namespaceDecl(hasName("omni"))),
      hasAncestor(namespaceDecl(hasName("detail"))),
      hasName("_reflected_indexed_type"),
      isTemplateInstantiation(),
      isDefinition()
      //
    );
  }

  using result = std::variant<reflectable, already_reflected, non_reflectable>;
  static auto resolve(const node_type &node,
    const clang::ASTUnit &ast,
    const std::set<refl::type_id> &resolved_types,
    bool print_debug = false) -> tl::expected<result, std::string>;
};

} // namespace match

namespace transforms {

// accumulated tu data
struct tu_data {
  // caching
  std::set<refl::type_id> resolved_types{};

  // reflected via `reflected_call`. type_id is needed to report on conflicts
  std::map<size_t, std::pair<refl::type_id, match::reflected_type_data>>
    indexed_types{};

  // reflected as dependency types: member fields, template parameters, member
  // typedefs and types
  std::map<refl::type_id, match::reflected_type_data> type_dependencies{};

  cli::verbosity_level _verbosity;
};

inline auto resolve_indexed_type_node(
  const match::reflected_indexed_type::node_type &n,
  const clang::ASTUnit &ast,
  const tu_data &tu_data)
  -> tl::expected<typename match::reflected_indexed_type::result, std::string> {
  return match::reflected_indexed_type::resolve(n,
    ast,
    tu_data.resolved_types,
    cli::print_debug(tu_data._verbosity));
}

auto fold_indexed_type_result(tu_data _accum,
  match::reflected_indexed_type::result _result)
  -> tl::expected<tu_data, std::string>;

} // namespace transforms

namespace codegen {

/*
 * fixme:
 *   struct type {
 *     struct nested {
 *       // this struct can't be forward-declared, but specialization can be
 *     };
 *   };
 *
 *   template <typename T>
 *   struct _reflected<struct type::nested, T> { ... };
 */
struct forward_declarable {
  std::string nm_qual_type;
  std::vector<std::string> fields;

  // todo: unions?
  bool is_class;

  friend bool operator<(const forward_declarable &lhs,
    const forward_declarable &rhs) noexcept {
    return lhs.nm_qual_type < rhs.nm_qual_type;
  }
};

struct reflection_data {
  std::set<forward_declarable, std::less<>> forward_declarable_types;
  std::map<size_t /*type_index*/, std::vector<std::string> /*fields*/>
    reflected_indexed_types;
};

struct options {};

tl::expected<reflection_data, std::string> prepare_data(
  header_mode::transforms::tu_data data);

tl::expected<void, std::string> emit_reflection_header_file(options,
  std::ostream &os,
  const reflection_data &data);

} // namespace codegen
} // namespace tool::header_mode
