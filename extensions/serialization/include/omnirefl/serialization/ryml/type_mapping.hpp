#pragma once

/** Default type mappings, customization registration, and dispatch.
 * See docs/CONVENTIONS.md#customization-points. */

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/traits.hpp>
#include <omnirefl/serialization/ryml/type_mapping/enum.hpp>
#include <omnirefl/serialization/ryml/type_mapping/to_record.hpp>
#include <omnirefl/serialization/ryml/type_mapping/sequence.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <type_traits>

namespace omni {
namespace ryml {

/** Non-recursive default type mappings may be used inside custom cp::to_type
 * and cp::from_type implementations. They instantiate neither
 * reflected_scope::mapping_to_type nor reflected_scope::mapping_from_type,
 * so delegation cannot recursively call the customization. */

/** Fundamental string-like mapping: reads scalar text without recursive
 * mapping_to_type dispatch. Direct calls from customizations avoid
 * re-entering mapping_to_type selection.
 *
 * @details String-view destinations refer to the tree's character storage;
 * the tree must remain alive and that storage must remain valid while used. */
struct to_string_like_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<is_read_state<State>,
      traits::is_string_like<T>>::value))
  auto operator()(::ryml::ConstNodeRef node, omni::type_t<T>,
    State context) const
    -> with_diagnostics_t<T, diagnostics>;

  /** Adapter for the value-returning overload: writes into *into and
   * returns diagnostics with a success marker. If .value is empty, the
   * destination state is unspecified and may contain partial updates. */
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<is_read_state<State>,
      traits::is_string_like<T>>::value))
  auto operator()(::ryml::ConstNodeRef node, T *into, State context) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;
};

constexpr to_string_like_t to_string_like{};

/** Fundamental string-like mapping: writes a quoted JSON/YAML string scalar
 * without recursive mapping_from_type dispatch. Direct calls from
 * customizations avoid re-entering mapping_from_type selection. */
struct from_string_like_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<is_write_state<State>,
      traits::is_string_like<T>>::value))
  auto operator()(const T &value, ::ryml::NodeRef node, State) const -> void;
};

constexpr from_string_like_t from_string_like{};

/** Fundamental boolean mapping: accepts unquoted true/false spellings. */
struct to_boolean_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (is_read_state<State>::value
      && std::is_same<bool, compat::remove_cv_t<T>>::value
      && !std::is_const<T>::value))
  auto operator()(::ryml::ConstNodeRef node, omni::type_t<T>,
    State context) const
    -> with_diagnostics_t<T, diagnostics>;

  /** Adapter for the value-returning overload: writes into *into and
   * returns diagnostics with a success marker. If .value is empty, the
   * destination state is unspecified and may contain partial updates. */
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (is_read_state<State>::value
      && std::is_same<bool, compat::remove_cv_t<T>>::value
      && !std::is_const<T>::value))
  auto operator()(::ryml::ConstNodeRef node, T *into, State context) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;
};

constexpr to_boolean_t to_boolean{};

/** Fundamental boolean mapping: writes a JSON/YAML boolean scalar. */
struct from_boolean_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (is_write_state<State>::value
      && std::is_same<bool, compat::remove_cv_t<T>>::value))
  void operator()(const T &value, ::ryml::NodeRef node, State) const;
};

constexpr from_boolean_t from_boolean{};

/** Fundamental integer mapping: validates signed/unsigned format and range.
 * Boolean destinations are handled by to_boolean. */
struct to_integer_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (is_read_state<State>::value && std::is_integral<T>::value
      && !std::is_same<bool, compat::remove_cv_t<T>>::value))
  auto operator()(::ryml::ConstNodeRef node, omni::type_t<T>,
    State context) const
    -> with_diagnostics_t<T, diagnostics>;

  /** Adapter for the value-returning overload: writes into *into and
   * returns diagnostics with a success marker. If .value is empty, the
   * destination state is unspecified and may contain partial updates. */
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (is_read_state<State>::value && std::is_integral<T>::value
      && !std::is_same<bool, compat::remove_cv_t<T>>::value))
  auto operator()(::ryml::ConstNodeRef node, T *into, State context) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;
};

constexpr to_integer_t to_integer{};

/** Fundamental integer mapping: writes a signed or unsigned integer scalar. */
struct from_integer_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (is_write_state<State>::value && std::is_integral<T>::value
      && !std::is_same<bool, compat::remove_cv_t<T>>::value))
  void operator()(const T &value, ::ryml::NodeRef node, State) const;
};

constexpr from_integer_t from_integer{};

/** Fundamental real-number mapping: validates floating-point format and
 * range. */
struct to_real_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (is_read_state<State>::value && std::is_floating_point<T>::value))
  auto operator()(::ryml::ConstNodeRef node, omni::type_t<T>,
    State context) const
    -> with_diagnostics_t<T, diagnostics>;

  /** Adapter for the value-returning overload: writes into *into and
   * returns diagnostics with a success marker. If .value is empty, the
   * destination state is unspecified and may contain partial updates. */
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (is_read_state<State>::value && std::is_floating_point<T>::value))
  auto operator()(::ryml::ConstNodeRef node, T *into, State context) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;
};

constexpr to_real_t to_real{};

/** Fundamental real-number mapping: writes a floating-point scalar with
 * round-trip precision. */
struct from_real_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (is_write_state<State>::value && std::is_floating_point<T>::value))
  void operator()(const T &value, ::ryml::NodeRef node, State) const;
};

constexpr from_real_t from_real{};

/** Arithmetic mapping selector: delegates to to_boolean, to_integer, or
 * to_real without mapping_to_type dispatch. */
struct to_arithmetic_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (is_read_state<State>::value && std::is_arithmetic<T>::value))
  auto operator()(::ryml::ConstNodeRef node, omni::type_t<T>,
    State context) const
    -> with_diagnostics_t<T, diagnostics>;

  /** Adapter for the value-returning overload: writes into *into and
   * returns diagnostics with a success marker. If .value is empty, the
   * destination state is unspecified and may contain partial updates. */
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (is_read_state<State>::value && std::is_arithmetic<T>::value))
  auto operator()(::ryml::ConstNodeRef node, T *into, State context) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;
};

constexpr to_arithmetic_t to_arithmetic{};

/** Arithmetic mapping selector: delegates to from_boolean, from_integer, or
 * from_real without mapping_from_type dispatch. */
struct from_arithmetic_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (is_write_state<State>::value && std::is_arithmetic<T>::value))
  auto operator()(const T &value, ::ryml::NodeRef node, State) const -> void;
};

constexpr from_arithmetic_t from_arithmetic{};

// FIXME: [high] Separate optional unwrapping from fundamental mappings:
// these optional-like mappings recursively invoke the type-mapping
// dispatchers for the contained value; they are not non-recursive defaults.

/** Reset on null; otherwise dispatch through mapping_to_type for the contained
 * value and emplace it on success. */
struct to_optional_like_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<is_read_state<State>,
      traits::is_optional<T>>::value))
  auto operator()(::ryml::ConstNodeRef node, T *into, State context) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;
};

constexpr to_optional_like_t to_optional_like{};

/** Write null for an empty optional; otherwise dispatch through
 * mapping_from_type for the contained value. */
struct from_optional_like_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<is_write_state<State>,
      traits::is_optional<T>>::value))
  auto operator()(const T &value, ::ryml::NodeRef node, State context) const
    -> void;
};

constexpr from_optional_like_t from_optional_like{};

/** Customization points for C++ type mappings. */
namespace cp {

// Callable variable-template registration is available starting with C++14.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
/** Callable variable-template customization, available since C++14.
 * The selected const object is copied when mapping a node. Configured default
 * compositors can be registered directly:
 *
```cpp
struct endpoint { int port; };

template <>
const auto omni::ryml::cp::to_type<endpoint> =
  omni::ryml::reflected_scope::to_record.with_name_by_alias(
    omni::ryml::alias_dict(std::make_tuple("port", "service_port")));
```
 *
 * @warning Keep specializations narrow: concrete types or specific template
 * families. Broad categories (e.g. all records or ranges) may overlap other
 * specializations and make lookup ambiguous. */
template <typename T, typename Enable = void>
constexpr traits::cp::unavailable to_type{};
#endif

/** C++11-compatible node-to-C++ customization. Unregistered types use the
 * default compositor. Register either this struct or the variable template.
 *
```cpp
struct counter { int value; };

template <>
struct omni::ryml::cp::to_type_t<counter> {
  OMNI_CONSTRAINED_TEMPLATE((typename State),
    (omni::ryml::is_read_state<State>::value))
  auto operator()(::ryml::ConstNodeRef node, counter *into,
    State context) const
    -> omni::ryml::with_diagnostics_t<omni::compat::monostate> {
    return omni::ryml::core::read_node(node, &into->value, context);
  }
};
```
 *
 * @warning Keep specializations narrow: concrete types or specific template
 * families. Broad categories (e.g. all records or ranges) may overlap other
 * specializations and make lookup ambiguous. */
template <typename T, typename Enable = void>
struct to_type_t: traits::cp::unavailable {};

// C++14 adds variable-template registration for serialization callables.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
/** C++-to-node callable customization. Register either this variable template
 * or from_type_t for a given type; the dispatcher selects a default otherwise.
 *
 * @warning Keep specializations narrow: concrete types or specific template
 * families. Broad categories (e.g. all records or ranges) may overlap other
 * specializations and make lookup ambiguous. */
template <typename T, typename Enable = void>
constexpr traits::cp::unavailable from_type{};
#endif

/** C++11-compatible C++-to-node customization. Unregistered types use a
 * default mapper.
 * A scalar wrapper can delegate conversion to its wrapped value:
 *
```cpp
struct counter { int value; };

template <>
struct omni::ryml::cp::from_type_t<counter> {
  OMNI_CONSTRAINED_TEMPLATE((typename State),
    (omni::ryml::is_write_state<State>::value))
  void operator()(const counter &value, ::ryml::NodeRef node,
    State context) const {
    omni::ryml::core::write_node(value.value, node, context);
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

// C++11 requires explicit return types for these dispatchers.
#if !defined(__cpp_return_type_deduction) \
  || 201304L > __cpp_return_type_deduction
namespace detail {

template <typename T>
struct to_type_cpp11_trailing_return;

template <typename T>
struct from_type_cpp11_trailing_return;

} // namespace detail
#endif

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
  traits::case_<traits::is_string_like<T>::value, to_string_like_t>,
  traits::case_<std::is_same<bool, T>::value, to_boolean_t>,
  traits::case_<std::is_integral<T>::value, to_integer_t>,
  traits::case_<std::is_floating_point<T>::value, to_real_t>,
  traits::case_<traits::is_optional<T>::value, to_optional_like_t>,
  traits::case_<omni::ryml::detail::is_mapped_range<T>::value, to_range_t>,
  traits::case_<omni::is_reflected_enum<T>::value,
    reflected_scope::to_enum_t<>>,
  traits::case_<omni::is_reflected_record<T>::value,
    reflected_scope::to_record_t<>>,
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
template <typename _T, typename T = compat::remove_cv_t<_T>>
using default_mapping_from_type = typename traits::select<
  traits::case_<traits::is_string_like<T>::value, from_string_like_t>,
  traits::case_<std::is_same<bool, T>::value, from_boolean_t>,
  traits::case_<std::is_integral<T>::value, from_integer_t>,
  traits::case_<std::is_floating_point<T>::value, from_real_t>,
  traits::case_<traits::is_optional<T>::value, from_optional_like_t>,
  traits::case_<omni::ryml::detail::is_mapped_range<const T>::value,
    from_range_t>,
  traits::case_<omni::is_reflected_enum<T>::value,
    reflected_scope::from_enum_t>,
  traits::case_<omni::is_reflected_record<T>::value,
    reflected_scope::from_record_t>,
  traits::cp::unavailable>::type;

} // namespace detail

/** Return the registered node-to-C++ callable by value, or the default
 * compositor when neither customization is registered. Selected callables
 * must be trivially copyable and copy-constructible. Registration conflicts
 * and missing mappings are diagnosed here, before invoking the mapper.
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

  static_assert(std::is_trivially_copyable<mapping_type>::value
      && std::is_copy_constructible<mapping_type>::value,
    "Node mapping callables must be trivially copyable and copy-constructible");

  return omni::ryml::detail::first_available(
    mapping_t, mapping_var, mapping_default);
}

/** Return the registered C++-to-node callable by value, or the default
 * compositor when neither customization is registered. Selected callables
 * must be trivially copyable and copy-constructible. Registration conflicts
 * and missing mappings are diagnosed here, before invoking the mapper.
 * @pre Metadata required by the selected mapping is available in reflected
 * scope; core entry points establish that scope before dispatch. */
template <typename T>
OMNI_CPP14_CONSTEXPR auto mapping_from_type(omni::type_t<T>)
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

  static_assert(std::is_trivially_copyable<mapping_type>::value
      && std::is_copy_constructible<mapping_type>::value,
    "Value mapping callables must be trivially copyable "
    "and copy-constructible");

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
// Static-assert sanity checks:
// extras/extensions/serialization/tests/type_mapping.cpp.
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
    cp::from_type_t<T>{},
    // C++11 has no variable-template registration.
    traits::cp::unavailable{},
    reflected_scope::detail::default_mapping_from_type<T>{}));
};

} // namespace detail
} // namespace ryml
} // namespace omni
#endif

#include <omnirefl/serialization/ryml/impl/type_mapping.hpp>

// TODO: [high] Define UTF-8 compliance for string mapping: conversion from
// non-char strings and handling of invalid UTF-8 in char-based strings.
// Use the shared operation customization points; unsupported encodings must
// produce a focused diagnostic rather than fall through to range mapping.
// See the disabled wide-string case in tests/string_mapping.cpp.
