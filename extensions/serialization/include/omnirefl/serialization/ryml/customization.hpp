#pragma once

/** Type mapping registration and customization lookup.
 * Signature notation: docs/CONVENTIONS.md#functional-composition-notation.
 * Registration: docs/CONVENTIONS.md#customization-points. */

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/traits.hpp>
#include <omnirefl/reflected_scope.hpp>
#include <omnirefl/serialization/ryml/impl/shared.hpp>
#include <omnirefl/serialization/ryml/map_node.hpp>
#include <omnirefl/serialization/ryml/map_type.hpp>
#include <omnirefl/serialization/ryml/state.hpp>
#include <omnirefl/traits.hpp>

#include <type_traits>

namespace omni {
namespace ryml {

/** Customization points for C++ type mappings. */
namespace cp {

#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
/** C++14-compatible node-to-type customization point for map_by_schema.
 *
```
cp::to_type<T> :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
```
 *
 * Provide either overload, or both. map_by_schema and deserialize prefer
 * type<T> when its result can be assigned. T* is a fallback when value
 * production is unavailable. A generic State parameter preserves child paths
 * and local selectors; its constraint is is_read_state.
 *
 * Prefer this variable template over to_type_t for customization. Only one
 * of the two may be specialized for a given type.
 * Unregistered types use the built-in mapping. The selected const callable
 * is copied for mapping.
 *
 * Example:
 *
```cpp
struct counter { int value; };

template <>
const auto omni::ryml::cp::to_type<counter> =
  [](auto state, ::ryml::ConstNodeRef node, omni::type_t<counter>)
    -> omni::ryml::with_diagnostics_t<counter> {
    return omni::ryml::map_integer_node(std::move(state),
      node,
      omni::type<decltype(counter::value)>)
      .transform([](auto value) -> counter { return {value}; });
  };
```
 *
 * @warning Keep specializations narrow: concrete types or specific template
 * families. Broad categories (e.g. all records or ranges) may overlap other
 * specializations and make lookup ambiguous. */
template <typename T, typename Enable = void>
constexpr traits::cp::unavailable to_type{};
#endif

/** C++11-compatible node-to-type customization point for map_by_schema.
 *
```
cp::to_type_t<T>{} :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
```
 *
 * Provide either overload, or both. map_by_schema and deserialize prefer
 * type<T> when its result can be assigned. T* is a fallback when value
 * production is unavailable. A generic State parameter preserves child paths
 * and local selectors; its constraint is is_read_state.
 *
 * Prefer the to_type variable template when C++14 or later is available.
 * Only one of the two may be specialized for a given type.
 * Unregistered types use the built-in mapping. The selected const callable
 * is copied for mapping.
 *
```cpp
struct counter { int value; };

template <>
struct omni::ryml::cp::to_type_t<counter> {
  OMNI_CONSTRAINED_TEMPLATE((typename State),
    (omni::ryml::is_read_state<State>::value))
  auto operator()(State state,
    ::ryml::ConstNodeRef node,
    omni::type_t<counter>) const -> omni::ryml::with_diagnostics_t<counter> {
    return omni::ryml::map_integer_node(std::move(state),
      node,
      omni::type_t<decltype(counter::value)>{})
      .transform([](decltype(counter::value) value) -> counter {
        return {value};
      });
  }
};
```
 *
 * @warning Keep specializations narrow: concrete types or specific template
 * families. Broad categories (e.g. all records or ranges) may overlap other
 * specializations and make lookup ambiguous. */
template <typename T, typename Enable = void>
struct to_type_t: traits::cp::unavailable {};

#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
/** C++14-compatible C++-to-node callable registration.
 * Register either this variable template or from_type_t for a given type;
 * the dispatcher selects a default otherwise.
 *
```
cp::from_type<T> :: (state, const T&) -> node
```
 *
 * State supplies the destination node through state.node(); returned nodes
 * borrow the active builder call and must not escape it.
 * A generic State parameter preserves child paths and local selectors;
 * its constraint is is_write_state.
 *
 * @warning Keep specializations narrow: concrete types or specific template
 * families. Broad categories (e.g. all records or ranges) may overlap other
 * specializations and make lookup ambiguous. */
template <typename T, typename Enable = void>
constexpr traits::cp::unavailable from_type{};
#endif

/** C++11-compatible C++-to-node customization. Unregistered types use a
 * default mapping function.
 *
```
cp::from_type_t<T> :: (state, const T&) -> node
```
 *
 * State supplies the destination through state.node(); returned nodes must
 * not escape the active builder call. A generic State preserves child paths
 * and local selectors; its constraint is is_write_state.
 * A scalar wrapper can delegate conversion to its wrapped value:
 *
```cpp
struct counter { int value; };

template <>
struct omni::ryml::cp::from_type_t<counter> {
  OMNI_CONSTRAINED_TEMPLATE((typename State),
    (omni::ryml::is_write_state<State>::value))
  auto operator()(State context,
    const counter &value) const -> ::ryml::NodeRef {
    return omni::ryml::map_integral(context, value.value);
  }
};
```
 *
 * @warning Keep specializations narrow: concrete types or specific template
 * families. Broad categories (e.g. all records or ranges) may overlap other
 * specializations and make lookup ambiguous. */
template <typename T, typename Enable = void>
struct from_type_t: traits::cp::unavailable {};

} // namespace cp

namespace reflected_scope {
namespace detail {

/** Default type mappings are selected separately from cp::to_type to avoid
 * partial-specialization ambiguity with user template-family mappings and
 * conflicts with user cp::to_type_t registrations.
 *
 * This alias selects the default callable type for reading a JSON/YAML tree
 * node into T.
 *
 * @details Record and enum selection requires generated metadata in reflected
 * scope. Without a default, this alias is traits::cp::unavailable. */
template <typename _T, typename T = compat::remove_cv_t<_T>>
using default_mapping_to_type = typename traits::select<
  traits::case_<traits::is_string_like<T>::value,
    omni::ryml::map_string_node_t<>>,
  traits::case_<std::is_same<bool, T>::value,
    map_boolean_node_t<>>,
  traits::case_<std::is_integral<T>::value,
    map_integer_node_t<>>,
  traits::case_<std::is_floating_point<T>::value,
    map_number_node_t<>>,
  traits::case_<traits::is_optional<T>::value,
    map_value_node_t<>>,
  traits::case_<omni::ryml::detail::is_mapped_range<T>::value,
    map_range_node_t<>>,
  traits::case_<omni::is_reflected_enum<T>::value,
    reflected_scope::map_node_to_enum_t<>>,
  traits::case_<omni::is_reflected_record<T>::value,
    reflected_scope::map_object_node_t<>>,
  traits::cp::unavailable>::type;

/** Default type mappings are selected separately from cp::from_type to avoid
 * partial-specialization ambiguity with user template-family mappings and
 * conflicts with user cp::from_type_t registrations.
 *
 * This alias selects the default callable type for writing T into a JSON/YAML
 * tree node.
 *
 * @details Record and enum selection requires generated metadata in reflected
 * scope. Without a default, this alias is traits::cp::unavailable. */
template <typename _T, typename T = compat::remove_cvref_t<_T>>
using default_mapping_from_type = typename traits::select<
  traits::case_<traits::is_string_like<T>::value, map_string_like_t>,
  traits::case_<std::is_same<bool, T>::value, map_boolean_t>,
  traits::case_<std::is_integral<T>::value, map_integral_t>,
  traits::case_<std::is_floating_point<T>::value, map_floating_point_t>,
  traits::case_<traits::is_optional<T>::value, map_optional_like_t>,
  traits::case_<omni::ryml::detail::is_mapped_range<const T>::value,
    map_range_t<>>,
  traits::case_<omni::is_reflected_enum<T>::value,
    reflected_scope::map_enum_t<>>,
  traits::case_<omni::is_reflected_record<T>::value,
    reflected_scope::map_record_t<>>,
  traits::cp::unavailable>::type;

} // namespace detail

/** Returns the registered node-to-C++ callable by value, or the default
 * compositor when neither customization is registered. Selected callables
 * must be copy-constructible. Registration conflicts and missing mappings
 * are diagnosed before invoking the mapping function.
 *
```
mapping_to_type :: type<T> -> {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
```
 *
 * The selected callable may provide either or both overloads. Mapping prefers
 * type<T> where assignment is possible; errors do not cause a T* retry.
 * @pre Metadata required by the selected mapping is available in reflected
 * scope; core entry points establish that scope before dispatch. */
template <typename T>
OMNI_CPP14_CONSTEXPR auto mapping_to_type(omni::type_t<T>)
  OMNI_CPP11_TRAILING_RETURN(
    typename omni::ryml::detail::to_type_cpp11_trailing_return<T>::type) {
  const cp::to_type_t<T> mapping_t{};
  const auto mapping_var =
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
    cp::to_type<T>;
#else
    traits::cp::unavailable{};
#endif

  static_assert(traits::cp::unavailable{} == mapping_t
      || traits::cp::unavailable{} == mapping_var,
    "Specialize either cp::to_type_t or cp::to_type, not both");

  const detail::default_mapping_to_type<T> mapping_default{};

  static_assert(traits::cp::unavailable{} != mapping_t
      || traits::cp::unavailable{} != mapping_var
      || traits::cp::unavailable{} != mapping_default,
    "No node mapping: specialize cp::to_type_t or cp::to_type");

  using mapping_type = decltype(omni::ryml::detail::first_available(
    mapping_t, mapping_var, mapping_default));

  static_assert(std::is_copy_constructible<mapping_type>::value,
    "Node mapping callables must be copy-constructible");

  return omni::ryml::detail::first_available(
    mapping_t, mapping_var, mapping_default);
}

/** Returns the registered C++-to-node callable by value, or the default
 * compositor when neither customization is registered. Selected callables
 * must be copy-constructible. Lookup ignores cv-qualifiers and references.
 * Registration conflicts and missing mappings are diagnosed before invoking
 * the mapping function.
 *
```
mapping_from_type :: type<T> -> ((state, const T&) -> node)
```
 *
 * @pre Metadata required by the selected mapping is available in reflected
 * scope; core entry points establish that scope before dispatch. */
template <typename Input, typename T = compat::remove_cvref_t<Input>>
OMNI_CPP14_CONSTEXPR auto mapping_from_type(omni::type_t<Input>)
  OMNI_CPP11_TRAILING_RETURN(
    typename omni::ryml::detail::from_type_cpp11_trailing_return<T>::type) {
  const cp::from_type_t<T> mapping_t{};
  const auto mapping_var =
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
    cp::from_type<T>;
#else
    traits::cp::unavailable{};
#endif

  static_assert(traits::cp::unavailable{} == mapping_t
      || traits::cp::unavailable{} == mapping_var,
    "Specialize either cp::from_type_t or cp::from_type, not both");

  const detail::default_mapping_from_type<T> mapping_default{};

  static_assert(traits::cp::unavailable{} != mapping_t
      || traits::cp::unavailable{} != mapping_var
      || traits::cp::unavailable{} != mapping_default,
    "No value mapping: specialize cp::from_type_t or cp::from_type");

  using mapping_type = decltype(omni::ryml::detail::first_available(
    mapping_t, mapping_var, mapping_default));

  static_assert(std::is_copy_constructible<mapping_type>::value,
    "Value mapping callables must be copy-constructible");

  return omni::ryml::detail::first_available(
    mapping_t, mapping_var, mapping_default);
}

} // namespace reflected_scope
} // namespace ryml
} // namespace omni

#if !defined(__cpp_return_type_deduction) \
  || 201304L > __cpp_return_type_deduction
namespace omni {
namespace ryml {
namespace detail {

// Ad hoc: C++11 cannot deduce function return types from their bodies.
// WARNING: keep these argument lists synchronized with mapping_to_type and
// mapping_from_type; default selection is shared through the aliases above.
template <typename T>
struct to_type_cpp11_trailing_return {
  using type = decltype(first_available(
    cp::to_type_t<T>{},
    // C++11 has no variable-template registration.
    traits::cp::unavailable{},
    reflected_scope::detail::default_mapping_to_type<T>{}));
};

template <typename T>
struct from_type_cpp11_trailing_return {
  using type = decltype(first_available(
    cp::from_type_t<compat::remove_cvref_t<T>>{},
    // C++11 has no variable-template registration.
    traits::cp::unavailable{},
    reflected_scope::detail::default_mapping_from_type<T>{}));
};

} // namespace detail
} // namespace ryml
} // namespace omni
#endif

namespace omni {
namespace ryml {
namespace reflected_scope {

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename Path, typename T),
  (is_schema_path<Path>::value))
OMNI_CPP14_CONSTEXPR auto mapping_to_type_t::operator()(Path,
  omni::type_t<T> type) const
  OMNI_CPP11_TRAILING_RETURN(
    typename omni::ryml::detail::to_type_cpp11_trailing_return<T>::type) {
  return reflected_scope::mapping_to_type(type);
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename Path, typename T),
  (is_schema_path<Path>::value))
OMNI_CPP14_CONSTEXPR auto mapping_from_type_t::operator()(Path,
  omni::type_t<T> type) const
  OMNI_CPP11_TRAILING_RETURN(
    typename omni::ryml::detail::from_type_cpp11_trailing_return<T>::type) {
  return reflected_scope::mapping_from_type(type);
}

namespace fn {

/** Default local mapping selector; see reflected_scope::mapping_to_type. */
#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr mapping_to_type_t mapping_to_type{};
#else
static constexpr const mapping_to_type_t &mapping_to_type =
  compat::detail::shared_state<mapping_to_type_t>::value;
#endif

/** Default local write-mapping selector; see
 * reflected_scope::mapping_from_type. */
#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr mapping_from_type_t mapping_from_type{};
#else
static constexpr const mapping_from_type_t &mapping_from_type =
  compat::detail::shared_state<mapping_from_type_t>::value;
#endif

} // namespace fn
} // namespace reflected_scope
} // namespace ryml
} // namespace omni
