#pragma once

/** Default type mappings, callable composition, and customization lookup.
 * Signature notation: docs/CONVENTIONS.md#functional-composition-notation.
 * Registration: docs/CONVENTIONS.md#customization-points. */

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/traits.hpp>
#include <omnirefl/reflected_scope.hpp>
#include <omnirefl/serialization/ryml/compose.hpp>
#include <omnirefl/serialization/ryml/type_mapping/enum.hpp>
#include <omnirefl/serialization/ryml/type_mapping/sequence.hpp>
#include <omnirefl/serialization/ryml/type_mapping/to_record.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <cassert>
#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {
namespace core {

/** Maps a scalar node's text to a string-like value.
 * Custom string APIs are supported through \ref adapt::assign_string.
 * Its callable object can be passed by value for C++11 composition.
 *
```text
map_node_to_string_like :: (node, type<T>, state) -> with_diagnostics<T>
```
 *
 * @pre Caller validates the node shape. node must refer to a scalar in a
 * valid tree; only a debug assertion checks the scalar-node precondition. */
struct map_node_to_string_like_t {
  // REFACTORME: [medium] Consider replacing ConstNodeRef with value-level
  // input; node validation belongs to composition.
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<traits::is_string_like<T>,
      is_read_state<State>>::value))
  auto operator()(::ryml::ConstNodeRef node, omni::type_t<T>, State state) const
    -> with_diagnostics_t<T, diagnostics> {
    assert(node.has_val() && !node.is_container());
    const auto scalar = node.val();
    using storage_type = compat::remove_cvref_t<decltype(adapt::as_string(
      std::declval<const T &>()))>;

    static_assert(std::is_same<char, typename storage_type::value_type>::value,
      "Ryml string mapping currently supports only char text.");

    // TODO: [medium] Define UTF-8 conversion for non-char strings and handling
    // of invalid UTF-8 through the shared operation customization points.
    // See the disabled wide-string case in tests/string_mapping.cpp.

    using view_type = compat::basic_string_view<
      typename storage_type::value_type, typename storage_type::traits_type>;

    T value{};
    adapt::assign_string(&value, view_type{scalar.str, scalar.len});
    return {
      /*value=*/std::move(value),
      /*diagnostics=*/{
        /*config=*/{state.config.error_policy, {}},
        /*issues=*/{},
        /*parse_error_message=*/{},
      },
    };
  }
};

constexpr map_node_to_string_like_t map_node_to_string_like{};

/** Boolean scalar-to-value mapping.
 *
```text
map_node_to_boolean :: (node, type<bool>, state) -> with_diagnostics<bool>
```
 *
 * @pre node refers to an unquoted true/false scalar in a valid tree.
 *
 * @details Uses \ref parse_boolean for conversion. The caller validates the
 * input; debug assertions check the precondition. */
struct map_node_to_boolean_t {
  // REFACTORME: [medium] Consider replacing ConstNodeRef with value-level
  // input; node validation belongs to composition.
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_same<bool, compat::remove_cv_t<T>>::value
      && !std::is_const<T>::value && is_read_state<State>::value))
  auto operator()(::ryml::ConstNodeRef node, omni::type_t<T>, State state) const
    -> with_diagnostics_t<T, diagnostics> {
    assert(node.has_val() && !node.is_container() && !node.is_val_quoted());
    const auto scalar = node.val();
    const auto result = parse_boolean({scalar.str, scalar.len});
    assert(result.has_value());
    return {
      /*value=*/ *result,
      /*diagnostics=*/{
        /*config=*/{state.config.error_policy, {}},
        /*issues=*/{},
        /*parse_error_message=*/{},
      },
    };
  }
};

constexpr map_node_to_boolean_t map_node_to_boolean{};

/** Maps a scalar node's text to an integer value.
 * Reports overflow through mapping diagnostics.
 *
```text
map_node_to_integer :: (node, type<T>, state) -> with_diagnostics<T>
```
 *
 * @pre Caller validates integer input in a valid tree; a debug assertion
 * checks this precondition. Destination range is checked during conversion. */
struct map_node_to_integer_t {
  // REFACTORME: [medium] Consider replacing ConstNodeRef with value-level
  // input; node validation belongs to composition.
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_integral<T>::value
      && !std::is_same<bool, compat::remove_cv_t<T>>::value
      && is_read_state<State>::value))
  auto operator()(::ryml::ConstNodeRef node, omni::type_t<T>, State state) const
    -> with_diagnostics_t<T, diagnostics> {
    assert(core::expect_integer_node(node).value);
    const auto expected = std::is_unsigned<T>::value
      ? json::value_kind::unsigned_integer : json::value_kind::integer;

    return omni::ryml::detail::read_scalar(
      node, omni::type_t<T>{}, state, expected);
  }
};

constexpr map_node_to_integer_t map_node_to_integer{};

/** Maps a scalar node's text to a real-number value.
 * Reports failed conversions through mapping diagnostics.
 *
```text
map_node_to_real :: (node, type<T>, state) -> with_diagnostics<T>
```
 *
 * @pre Caller validates numeric input in a valid tree; a debug assertion
 * checks this precondition. */
struct map_node_to_real_t {
  // REFACTORME: [medium] Consider replacing ConstNodeRef with value-level
  // input; node validation belongs to composition.
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_floating_point<T>::value && is_read_state<State>::value))
  auto operator()(::ryml::ConstNodeRef node, omni::type_t<T>, State state) const
    -> with_diagnostics_t<T, diagnostics> {
    assert(core::expect_number_node(node).value);
    return omni::ryml::detail::read_scalar(node, omni::type_t<T>{}, state,
      json::value_kind::number);
  }
};

constexpr map_node_to_real_t map_node_to_real{};

/** Maps a scalar node's text to the requested arithmetic type.
 * Provides a common entry point for Boolean, integer, and real mappings.
 *
```text
map_node_to_arithmetic :: (node, type<T>, state) -> with_diagnostics<T>
```
 *
 * @pre Caller validates the node shape. node must refer to a scalar in a
 * valid tree; only a debug assertion checks the scalar-node precondition. */
struct map_node_to_arithmetic_t {
  // REFACTORME: [medium] Consider replacing ConstNodeRef with value-level
  // input; node validation belongs to composition.
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_arithmetic<T>::value && is_read_state<State>::value))
  auto operator()(::ryml::ConstNodeRef node, omni::type_t<T>, State state) const
    -> with_diagnostics_t<T, diagnostics> {
    assert(node.has_val() && !node.is_container());
    const auto read = typename traits::select<
      traits::case_<std::is_same<bool, compat::remove_cv_t<T>>::value,
        core::map_node_to_boolean_t>,
      traits::case_<std::is_integral<T>::value, core::map_node_to_integer_t>,
      core::map_node_to_real_t>::type{};

    return read(node, omni::type_t<T>{}, state);
  }
};

constexpr map_node_to_arithmetic_t map_node_to_arithmetic{};

} // namespace core

/** Non-recursive fundamental type mappings may be used inside custom
 * cp::to_type and cp::from_type implementations. They call neither
 * \ref reflected_scope::mapping_to_type nor
 * \ref reflected_scope::mapping_from_type, avoiding recursive selection of
 * the same customization. Optional mappings below dispatch contained values. */

/** Creates a string-like node-to-type mapping function.
 * Checks the node type before calling the core mapping function.
 * Does not call \ref reflected_scope::mapping_to_type.
 *
```text
to_string_like :: type<T> -> {
  (node, type<T>, state) -> with_diagnostics<T>,
  (node, T*, state)      -> with_diagnostics<monostate>
}
```
 *
 * @details String-view destinations borrow the tree's character storage.
 * The tree and its character storage must remain valid while views are used. */
template <typename T>
constexpr auto to_string_like(omni::type_t<T>)
  -> decltype(compose::with_mutating_node_to_type_overload(
    compose::with_node_check<core::expect_string_node>(
      core::map_node_to_string_like))) {
  return compose::with_mutating_node_to_type_overload(
    compose::with_node_check<core::expect_string_node>(
      core::map_node_to_string_like));
}

/** Maps a string-like value to a quoted JSON/YAML scalar.
 * Does not call \ref reflected_scope::mapping_from_type.
 * Custom string APIs are supported through \ref adapt::as_string.
 * Converted text is copied; reference-tree writing may borrow source views.
 *
```text
from_string_like :: (const T&, node, state) -> void
```
 */
struct from_string_like_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<traits::is_string_like<T>,
      is_write_state<State>>::value))
  auto operator()(const T &value, ::ryml::NodeRef node, State) const -> void;
};

constexpr from_string_like_t from_string_like{};

/** Creates a boolean node-to-type mapping function.
 * Checks the node type before calling the core mapping function.
 * Does not call \ref reflected_scope::mapping_to_type.
 *
```text
to_boolean :: type<T> -> {
  (node, type<T>, state) -> with_diagnostics<T>,
  (node, T*, state)      -> with_diagnostics<monostate>
}
```
 */
template <typename T>
constexpr auto to_boolean(omni::type_t<T>)
  -> decltype(compose::with_mutating_node_to_type_overload(
    compose::with_node_check<core::expect_boolean_node>(
      core::map_node_to_boolean))) {
  return compose::with_mutating_node_to_type_overload(
    compose::with_node_check<core::expect_boolean_node>(
      core::map_node_to_boolean));
}

/** Maps a Boolean value to a JSON/YAML scalar.
 * Does not call \ref reflected_scope::mapping_from_type.
 *
```text
from_boolean :: (T, node, state) -> void
```
 */
struct from_boolean_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_same<bool, compat::remove_cv_t<T>>::value
      && is_write_state<State>::value))
  void operator()(const T &value, ::ryml::NodeRef node, State) const;
};

constexpr from_boolean_t from_boolean{};

/** Creates an integer node-to-type mapping function.
 * Checks the node type before calling the core mapping function.
 * Does not call \ref reflected_scope::mapping_to_type.
 *
```text
to_integer :: type<T> -> {
  (node, type<T>, state) -> with_diagnostics<T>,
  (node, T*, state)      -> with_diagnostics<monostate>
}
```
 */
template <typename T>
constexpr auto to_integer(omni::type_t<T>)
  -> decltype(compose::with_mutating_node_to_type_overload(
    compose::with_node_check<core::expect_integer_node>(
      core::map_node_to_integer))) {
  return compose::with_mutating_node_to_type_overload(
    compose::with_node_check<core::expect_integer_node>(
      core::map_node_to_integer));
}

/** Maps a integer value to a JSON/YAML scalar.
 * Does not call \ref reflected_scope::mapping_from_type.
 *
```text
from_integer :: (const T&, node, state) -> void
```
 */
struct from_integer_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_integral<T>::value
      && !std::is_same<bool, compat::remove_cv_t<T>>::value
      && is_write_state<State>::value))
  void operator()(const T &value, ::ryml::NodeRef node, State) const;
};

constexpr from_integer_t from_integer{};

/** Creates a real-number node-to-type mapping function.
 * Checks the node type before calling the core mapping function.
 * Does not call \ref reflected_scope::mapping_to_type.
 *
```text
to_real :: type<T> -> {
  (node, type<T>, state) -> with_diagnostics<T>,
  (node, T*, state)      -> with_diagnostics<monostate>
}
```
 */
template <typename T>
constexpr auto to_real(omni::type_t<T>)
  -> decltype(compose::with_mutating_node_to_type_overload(
    compose::with_node_check<core::expect_number_node>(
      core::map_node_to_real))) {
  return compose::with_mutating_node_to_type_overload(
    compose::with_node_check<core::expect_number_node>(
      core::map_node_to_real));
}

/** Maps a real-number value to a JSON/YAML scalar.
 * Does not call \ref reflected_scope::mapping_from_type.
 * Preserves enough significant digits for round-trip conversion.
 *
```text
from_real :: (const T&, node, state) -> void
```
 */
struct from_real_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_floating_point<T>::value && is_write_state<State>::value))
  void operator()(const T &value, ::ryml::NodeRef node, State) const;
};

constexpr from_real_t from_real{};

/** Creates an arithmetic node-to-type mapping function.
 * Checks the node type before calling the core mapping function.
 * Does not call \ref reflected_scope::mapping_to_type.
 *
```text
to_arithmetic :: type<T> -> {
  (node, type<T>, state) -> with_diagnostics<T>,
  (node, T*, state)      -> with_diagnostics<monostate>
}
```
 */
template <typename T>
constexpr auto to_arithmetic(omni::type_t<T>)
  -> typename traits::select<
    traits::case_<std::is_same<bool, compat::remove_cv_t<T>>::value,
      decltype(to_boolean(omni::type_t<T>{}))>,
    traits::case_<std::is_integral<T>::value,
      decltype(to_integer(omni::type_t<T>{}))>,
    decltype(to_real(omni::type_t<T>{}))>::type {
  return {};
}

/** Maps an arithmetic value to a Boolean, integer, or real JSON/YAML scalar.
 * Does not call \ref reflected_scope::mapping_from_type.
 *
```text
from_arithmetic :: (const T&, node, state) -> void
```
 */
struct from_arithmetic_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_arithmetic<T>::value && is_write_state<State>::value))
  auto operator()(const T &value, ::ryml::NodeRef node, State) const -> void;
};

constexpr from_arithmetic_t from_arithmetic{};

// FIXME: [high] Separate optional unwrapping from fundamental mappings:
// these optional-like mappings recursively invoke the type-mapping
// dispatchers for the contained value; they are not non-recursive defaults.

/** Maps a node into an optional-like destination.
 * Null resets the destination. Non-null input uses the contained type's
 * registered mapping and is emplaced only on success. */
struct to_optional_like_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<traits::is_optional<T>, is_read_state<State>>::value))
  auto operator()(::ryml::ConstNodeRef node, T *into, State context) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;
};

/** Creates an optional-like node-to-type mapping function.
 * Null resets the destination; non-null values dispatch through
 * \ref reflected_scope::mapping_to_type for the contained type.
 *
```text
to_optional_like :: type<T> ->
  (node, T*, state) -> with_diagnostics<monostate>
```
 */
template <typename T>
constexpr auto to_optional_like(omni::type_t<T>)
  -> decltype(compose::with_node_check<core::expect_value_node>(
    to_optional_like_t{})) {
  return compose::with_node_check<core::expect_value_node>(
    to_optional_like_t{});
}

/** Maps an empty optional to null, or maps its contained value to a node.
 * Contained values use \ref reflected_scope::mapping_from_type.
 *
```text
from_optional_like :: (const T&, node, state) -> void
```
 */
struct from_optional_like_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<traits::is_optional<T>, is_write_state<State>>::value))
  auto operator()(const T &value, ::ryml::NodeRef node, State context) const
    -> void;
};

constexpr from_optional_like_t from_optional_like{};

/** Customization points for C++ type mappings. */
namespace cp {

// Callable variable-template registration is available starting with C++14.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
/** C++14-compatible node-to-C++ callable registration.
 * The selected const object is copied when mapping a node. Register either
 * this variable template or to_type_t for a given type.
 *
```text
cp::to_type<T> :: (node, T*, state) -> with_diagnostics<monostate>
```
 *
 * A scalar wrapper can delegate to a default mapping without repeating lookup:
 *
```cpp
struct counter { int value; };

template <>
const auto omni::ryml::cp::to_type<counter> =
  [](::ryml::ConstNodeRef node, counter *into, auto state) {
    return omni::ryml::to_integer(omni::type_t<int>{})(
      node, &into->value, state);
  };
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
    return omni::ryml::to_integer(omni::type_t<int>{})(
      node, &into->value, context);
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
/** C++14-compatible C++-to-node callable registration.
 * Register either this variable template or from_type_t for a given type;
 * the dispatcher selects a default otherwise.
 *
```text
cp::from_type<T> :: (const T&, node, state) -> void
```
 *
 * @warning Keep specializations narrow: concrete types or specific template
 * families. Broad categories (e.g. all records or ranges) may overlap other
 * specializations and make lookup ambiguous. */
template <typename T, typename Enable = void>
constexpr traits::cp::unavailable from_type{};
#endif

/** C++11-compatible C++-to-node customization. Unregistered types use a
 * default mapping function.
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
    omni::ryml::from_integer(value.value, node, context);
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
  traits::case_<traits::is_string_like<T>::value,
    decltype(to_string_like(omni::type_t<T>{}))>,
  traits::case_<std::is_same<bool, T>::value,
    decltype(to_boolean(omni::type_t<T>{}))>,
  traits::case_<std::is_integral<T>::value,
    decltype(to_integer(omni::type_t<T>{}))>,
  traits::case_<std::is_floating_point<T>::value,
    decltype(to_real(omni::type_t<T>{}))>,
  traits::case_<traits::is_optional<T>::value,
    decltype(to_optional_like(omni::type_t<T>{}))>,
  traits::case_<omni::ryml::detail::is_mapped_range<T>::value,
    decltype(to_range(omni::type_t<T>{}))>,
  traits::case_<omni::is_reflected_enum<T>::value,
    decltype(reflected_scope::to_enum(omni::type_t<T>{}))>,
  traits::case_<omni::is_reflected_record<T>::value,
    decltype(reflected_scope::to_record(omni::type_t<T>{}))>,
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

/** Returns the registered node-to-C++ callable by value, or the default
 * compositor when neither customization is registered. Selected callables
 * must be trivially copyable and copy-constructible. Registration conflicts
 * and missing mappings are diagnosed before invoking the mapping function.
 *
```text
mapping_to_type :: type<T> ->
  ((node, T*, state) -> with_diagnostics<monostate>)
```
 *
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

/** Returns the registered C++-to-node callable by value, or the default
 * compositor when neither customization is registered. Selected callables
 * must be trivially copyable and copy-constructible. Registration conflicts
 * and missing mappings are diagnosed before invoking the mapping function.
 *
```text
mapping_from_type :: type<T> -> ((const T&, node, state) -> void)
```
 *
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
