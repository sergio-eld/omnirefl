#pragma once

/** Built-in type mappings and customization lookup.
 * Signature notation: docs/CONVENTIONS.md#functional-composition-notation.
 * Registration: docs/CONVENTIONS.md#customization-points. */

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/traits.hpp>
#include <omnirefl/traits.hpp>
#include <omnirefl/reflected_scope.hpp>
#include <omnirefl/serialization/ryml/compose.hpp>
#include <omnirefl/serialization/ryml/core.hpp>
#include <omnirefl/serialization/ryml/type_mapping/enum.hpp>
#include <omnirefl/serialization/ryml/type_mapping/sequence.hpp>
#include <omnirefl/serialization/ryml/type_mapping/record.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {

/** Non-recursive fundamental type mappings may be used inside custom
 * cp::to_type and cp::from_type implementations. They call neither
 * \ref reflected_scope::mapping_to_type nor
 * \ref reflected_scope::mapping_from_type, avoiding recursive selection of
 * the same customization. Optional mappings below dispatch contained values. */

/** Checked char-based string-like node-to-type mapping.
 * Checks the node type before calling the core mapping function.
 * Does not call \ref reflected_scope::mapping_to_type.
 *
```
default_to_string_like :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
```
 *
 * @details String-view destinations borrow the tree's character storage.
 * The tree and its character storage must remain valid while views are used. */
template <typename = void>
struct default_to_string_like_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (traits::is_string_like<T>::value && is_read_state<State>::value))
  auto operator()(State state,
    ::ryml::ConstNodeRef node,
    omni::type_t<T> type) const -> with_diagnostics_t<T> {
    return compose::with_node_check<core::expect_string_node>(
      core::map_node_to_string_like)(std::move(state), node, type);
  }

  /** (QoL) Mutating adapter for the value-producing overload. */
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (traits::is_string_like<T>::value && is_read_state<State>::value
      && std::is_assignable<T &, T &&>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate> {
    return compose::with_mutating_node_to_type_overload(
      compose::with_node_check<core::expect_string_node>(
        core::map_node_to_string_like))(std::move(state), node, into);
  }

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
  // Ad hoc: pre-C++17 shared identity; constexpr definition needs a
  // complete self-type and therefore belongs in the implementation header.
  static const default_to_string_like_t _default;
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr default_to_string_like_t<> default_to_string_like{};
#else
static constexpr const default_to_string_like_t<> &default_to_string_like =
  default_to_string_like_t<>::_default;
#endif

/** Maps a string-like value to a quoted JSON/YAML scalar.
 * Does not call \ref reflected_scope::mapping_from_type.
 * Custom string APIs are supported through \ref adapt::as_string.
 * Converted text is copied; reference-tree writing may borrow source views.
 *
```
from_string_like :: (state, const T&, node) -> void
```
 */
template <typename = void>
struct from_string_like_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<traits::is_string_like<T>,
      is_write_state<State>>::value))
  auto operator()(State, const T &value, ::ryml::NodeRef node) const -> void;

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
  // Ad hoc: pre-C++17 shared identity; constexpr definition needs a
  // complete self-type and therefore belongs in the implementation header.
  static const from_string_like_t _default;
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr from_string_like_t<> from_string_like{};
#else
static constexpr const from_string_like_t<> &from_string_like =
  from_string_like_t<>::_default;
#endif

/** Checked Boolean node-to-type mapping.
 * Checks the node type before calling the core mapping function.
 * Does not call \ref reflected_scope::mapping_to_type.
 *
```
default_to_boolean :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
```
 */
template <typename = void>
struct default_to_boolean_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_same<bool, compat::remove_cv_t<T>>::value
      && !std::is_const<T>::value && is_read_state<State>::value))
  auto operator()(State state,
    ::ryml::ConstNodeRef node,
    omni::type_t<T> type) const -> with_diagnostics_t<T> {
    return compose::with_node_check<core::expect_boolean_node>(
      core::map_node_to_boolean)(std::move(state), node, type);
  }

  /** (QoL) Mutating adapter for the value-producing overload. */
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_same<bool, compat::remove_cv_t<T>>::value
      && !std::is_const<T>::value && is_read_state<State>::value
      && std::is_assignable<T &, T &&>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate> {
    return compose::with_mutating_node_to_type_overload(
      compose::with_node_check<core::expect_boolean_node>(
        core::map_node_to_boolean))(std::move(state), node, into);
  }

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
  // Ad hoc: pre-C++17 shared identity; constexpr definition needs a
  // complete self-type and therefore belongs in the implementation header.
  static const default_to_boolean_t _default;
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr default_to_boolean_t<> default_to_boolean{};
#else
static constexpr const default_to_boolean_t<> &default_to_boolean =
  default_to_boolean_t<>::_default;
#endif

/** Maps a Boolean value to a JSON/YAML scalar.
 * Does not call \ref reflected_scope::mapping_from_type.
 *
```
from_boolean :: (state, T, node) -> void
```
 */
template <typename = void>
struct from_boolean_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_same<bool, compat::remove_cv_t<T>>::value
      && is_write_state<State>::value))
  void operator()(State, const T &value, ::ryml::NodeRef node) const;

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
  // Ad hoc: pre-C++17 shared identity; constexpr definition needs a
  // complete self-type and therefore belongs in the implementation header.
  static const from_boolean_t _default;
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr from_boolean_t<> from_boolean{};
#else
static constexpr const from_boolean_t<> &from_boolean =
  from_boolean_t<>::_default;
#endif

/** Checked integer node-to-type mapping.
 * Checks the node type before calling the core mapping function.
 * Does not call \ref reflected_scope::mapping_to_type.
 *
```
default_to_integral :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
```
 */
template <typename = void>
struct default_to_integral_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_integral<T>::value
      && !std::is_same<bool, compat::remove_cv_t<T>>::value
      && is_read_state<State>::value))
  auto operator()(State state,
    ::ryml::ConstNodeRef node,
    omni::type_t<T> type) const -> with_diagnostics_t<T> {
    return compose::with_node_check<core::expect_integer_node>(
      core::map_node_to_integral)(std::move(state), node, type);
  }

  /** (QoL) Mutating adapter for the value-producing overload. */
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_integral<T>::value
      && !std::is_same<bool, compat::remove_cv_t<T>>::value
      && is_read_state<State>::value
      && std::is_assignable<T &, T &&>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate> {
    return compose::with_mutating_node_to_type_overload(
      compose::with_node_check<core::expect_integer_node>(
        core::map_node_to_integral))(std::move(state), node, into);
  }

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
  // Ad hoc: pre-C++17 shared identity; constexpr definition needs a
  // complete self-type and therefore belongs in the implementation header.
  static const default_to_integral_t _default;
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr default_to_integral_t<> default_to_integral{};
#else
static constexpr const default_to_integral_t<> &default_to_integral =
  default_to_integral_t<>::_default;
#endif

/** Maps an integral value to a JSON/YAML scalar.
 * Does not call \ref reflected_scope::mapping_from_type.
 *
```
from_integral :: (state, const T&, node) -> void
```
 */
template <typename = void>
struct from_integral_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_integral<T>::value
      && !std::is_same<bool, compat::remove_cv_t<T>>::value
      && is_write_state<State>::value))
  void operator()(State, const T &value, ::ryml::NodeRef node) const;

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
  // Ad hoc: pre-C++17 shared identity; constexpr definition needs a
  // complete self-type and therefore belongs in the implementation header.
  static const from_integral_t _default;
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr from_integral_t<> from_integral{};
#else
static constexpr const from_integral_t<> &from_integral =
  from_integral_t<>::_default;
#endif

/** Checked floating-point node-to-type mapping.
 * Checks the node type before calling the core mapping function.
 * Does not call \ref reflected_scope::mapping_to_type.
 *
```
default_to_floating_point :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
```
 */
template <typename = void>
struct default_to_floating_point_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_floating_point<T>::value && is_read_state<State>::value))
  auto operator()(State state,
    ::ryml::ConstNodeRef node,
    omni::type_t<T> type) const -> with_diagnostics_t<T> {
    return compose::with_node_check<core::expect_number_node>(
      core::map_node_to_floating_point)(std::move(state), node, type);
  }

  /** (QoL) Mutating adapter for the value-producing overload. */
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_floating_point<T>::value && is_read_state<State>::value
      && std::is_assignable<T &, T &&>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate> {
    return compose::with_mutating_node_to_type_overload(
      compose::with_node_check<core::expect_number_node>(
        core::map_node_to_floating_point))(std::move(state), node, into);
  }

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
  // Ad hoc: pre-C++17 shared identity; constexpr definition needs a
  // complete self-type and therefore belongs in the implementation header.
  static const default_to_floating_point_t _default;
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr default_to_floating_point_t<> default_to_floating_point{};
#else
static constexpr const default_to_floating_point_t<> &default_to_floating_point =
  default_to_floating_point_t<>::_default;
#endif

/** Maps a floating-point value to a JSON/YAML scalar.
 * Does not call \ref reflected_scope::mapping_from_type.
 * Preserves enough significant digits for round-trip conversion.
 *
```
from_floating_point :: (state, const T&, node) -> void
```
 */
template <typename = void>
struct from_floating_point_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_floating_point<T>::value && is_write_state<State>::value))
  void operator()(State, const T &value, ::ryml::NodeRef node) const;

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
  // Ad hoc: pre-C++17 shared identity; constexpr definition needs a
  // complete self-type and therefore belongs in the implementation header.
  static const from_floating_point_t _default;
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr from_floating_point_t<> from_floating_point{};
#else
static constexpr const from_floating_point_t<> &from_floating_point =
  from_floating_point_t<>::_default;
#endif

/** Checked arithmetic node-to-type mapping.
 * Checks the node type before calling the core mapping function.
 * Does not call \ref reflected_scope::mapping_to_type.
 * Boolean destinations require a non-const type. Pointer destinations must
 * be assignable from the mapped value.
 *
```
default_to_arithmetic :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
```
 */
template <typename = void>
struct default_to_arithmetic_t {
  // Validate the category required by T, rather than accepting every scalar.
  template <typename T>
  using mapping_type = typename traits::select<
    traits::case_<std::is_same<bool, compat::remove_cv_t<T>>::value,
      default_to_boolean_t<>>,
    traits::case_<std::is_integral<T>::value, default_to_integral_t<>>,
    default_to_floating_point_t<>>::type;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_arithmetic<T>::value && is_read_state<State>::value
      && (!std::is_const<T>::value
        || !std::is_same<bool, compat::remove_cv_t<T>>::value)))
  auto operator()(State state,
    ::ryml::ConstNodeRef node,
    omni::type_t<T> type) const -> with_diagnostics_t<T> {
    return mapping_type<T>{}(std::move(state), node, type);
  }

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_arithmetic<T>::value && is_read_state<State>::value
      && std::is_assignable<T &, T &&>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate> {
    return mapping_type<T>{}(std::move(state), node, into);
  }

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
  // Ad hoc: pre-C++17 shared identity; constexpr definition needs a
  // complete self-type and therefore belongs in the implementation header.
  static const default_to_arithmetic_t _default;
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr default_to_arithmetic_t<> default_to_arithmetic{};
#else
static constexpr const default_to_arithmetic_t<> &default_to_arithmetic =
  default_to_arithmetic_t<>::_default;
#endif

/** Maps an arithmetic value to a Boolean, integer, or real JSON/YAML scalar.
 * Does not call \ref reflected_scope::mapping_from_type.
 *
```
from_arithmetic :: (state, const T&, node) -> void
```
 */
template <typename = void>
struct from_arithmetic_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_arithmetic<T>::value && is_write_state<State>::value))
  auto operator()(State, const T &value, ::ryml::NodeRef node) const -> void;

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
  // Ad hoc: pre-C++17 shared identity; constexpr definition needs a
  // complete self-type and therefore belongs in the implementation header.
  static const from_arithmetic_t _default;
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr from_arithmetic_t<> from_arithmetic{};
#else
static constexpr const from_arithmetic_t<> &from_arithmetic =
  from_arithmetic_t<>::_default;
#endif

// FIXME: [high] Separate optional unwrapping from fundamental mappings:
// these optional-like mappings recursively invoke the type-mapping
// dispatchers for the contained value; they are not non-recursive defaults.

/** Maps a node into an optional-like destination.
 * Null resets the destination. Non-null input uses the contained type's
 * registered mapping and is emplaced only on success. */
struct to_optional_like_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<traits::is_optional<T>, is_read_state<State>>::value))
  auto operator()(State context, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;
};

/** Creates an optional-like node-to-type mapping function.
 * Null resets the destination; non-null values dispatch through
 * \ref reflected_scope::mapping_to_type for the contained type.
 *
```
default_to_optional_like :: type<T> ->
  (state, node, T*) -> with_diagnostics<monostate>
```
 */
template <typename T>
constexpr auto default_to_optional_like(omni::type_t<T>)
  -> compose::detail::with_node_check_t<core::expect_value_node,
    to_optional_like_t> {
  return {};
}

/** Maps an empty optional to null, or maps its contained value to a node.
 * Contained values use \ref reflected_scope::mapping_from_type.
 *
```
from_optional_like :: (state, const T&, node) -> void
```
 */
template <typename = void>
struct from_optional_like_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<traits::is_optional<T>, is_write_state<State>>::value))
  auto operator()(State context, const T &value, ::ryml::NodeRef node) const
    -> void;

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
  // Ad hoc: pre-C++17 shared identity; constexpr definition needs a
  // complete self-type and therefore belongs in the implementation header.
  static const from_optional_like_t _default;
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr from_optional_like_t<> from_optional_like{};
#else
static constexpr const from_optional_like_t<> &from_optional_like =
  from_optional_like_t<>::_default;
#endif

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
 * production is unavailable.
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
    return omni::ryml::default_to_integral(std::move(state),
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
 * production is unavailable.
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
    return omni::ryml::default_to_integral(std::move(state),
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
cp::from_type<T> :: (state, const T&, node) -> void
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
  void operator()(State context,
    const counter &value,
    ::ryml::NodeRef node) const {
    omni::ryml::from_integral(context, value.value, node);
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
    default_to_string_like_t<>>,
  traits::case_<std::is_same<bool, T>::value,
    default_to_boolean_t<>>,
  traits::case_<std::is_integral<T>::value,
    default_to_integral_t<>>,
  traits::case_<std::is_floating_point<T>::value,
    default_to_floating_point_t<>>,
  traits::case_<traits::is_optional<T>::value,
    decltype(default_to_optional_like(omni::type_t<T>{}))>,
  traits::case_<omni::ryml::detail::is_mapped_range<T>::value,
    decltype(default_to_range(omni::type_t<T>{}))>,
  traits::case_<omni::is_reflected_enum<T>::value,
    decltype(reflected_scope::default_to_enum(omni::type_t<T>{}))>,
  traits::case_<omni::is_reflected_record<T>::value,
    decltype(reflected_scope::default_to_record(omni::type_t<T>{}))>,
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
  traits::case_<traits::is_string_like<T>::value, from_string_like_t<>>,
  traits::case_<std::is_same<bool, T>::value, from_boolean_t<>>,
  traits::case_<std::is_integral<T>::value, from_integral_t<>>,
  traits::case_<std::is_floating_point<T>::value, from_floating_point_t<>>,
  traits::case_<traits::is_optional<T>::value, from_optional_like_t<>>,
  traits::case_<omni::ryml::detail::is_mapped_range<const T>::value,
    from_range_t<>>,
  traits::case_<omni::is_reflected_enum<T>::value,
    reflected_scope::from_enum_t<>>,
  traits::case_<omni::is_reflected_record<T>::value,
    reflected_scope::from_record_t<>>,
  traits::cp::unavailable>::type;

} // namespace detail

/** Returns the registered node-to-C++ callable by value, or the default
 * compositor when neither customization is registered. Selected callables
 * must be trivially copyable and copy-constructible. Registration conflicts
 * and missing mappings are diagnosed before invoking the mapping function.
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
```
mapping_from_type :: type<T> -> ((state, const T&, node) -> void)
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
