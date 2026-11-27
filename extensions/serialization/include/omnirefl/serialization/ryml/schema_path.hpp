#pragma once

/** Compile-time schema locations passed to mapping selectors. */

#include <omnirefl/extensions/static_string.hpp>
#include <omnirefl/functional.hpp>

namespace omni {
namespace ryml {

/** Schema location with its original root and containing record or range.
 * The parent is void at the root. Optionals retain their parent and path;
 * container values append [] independently of document keys and indices.
 * Each type is exposed as an alias and a type token. */
template <typename Root, typename Parent, typename String>
struct schema_path_t final {
  using enclosing_root_t = Root;
  using schema_parent_t = Parent;

  static constexpr omni::type_t<Root> enclosing_root{};
  static constexpr omni::type_t<Parent> schema_parent{};

  /** Compile-time string supporting starts_with, ends_with, and contains.
   * Characters are materialized only when this accessor is used. */
  static constexpr auto str() OMNI_CPP11_TRAILING_RETURN(String) {
#if OMNI_HAS_NTTP_STRING
    return nttp_string<omni::detail::static_string_literal<
      String::size() + 1>{String::view()}>{};
#else
    return String{};
#endif
  }
};

// Static constexpr members need a definition when odr-used before C++17.
#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
template <typename Root, typename Parent, typename String>
constexpr omni::type_t<Root>
  schema_path_t<Root, Parent, String>::enclosing_root;

template <typename Root, typename Parent, typename String>
constexpr omni::type_t<Parent>
  schema_path_t<Root, Parent, String>::schema_parent;
#endif

/** Whether T is a schema_path_t specialization, ignoring cv/ref qualifiers. */
template <typename T>
struct is_schema_path:
    compat::bool_constant<traits::is_specialization_of<schema_path_t, T>()> {};

// C++14 adds variable templates for trait values.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <typename T>
constexpr bool is_schema_path_v = is_schema_path<T>::value;
#endif

// C++20 callers can constrain mapping selectors without spelling path types.
#if defined(__cpp_concepts) && 201907L <= __cpp_concepts
template <typename T>
concept schema_path = is_schema_path<T>::value;
#endif

} // namespace ryml
} // namespace omni
