#pragma once

/** Built-in type mappings and customization lookup.
 * Signature notation: docs/CONVENTIONS.md#functional-composition-notation.
 * Registration: docs/CONVENTIONS.md#customization-points. */

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/mapping_strategy.hpp>
#include <omnirefl/extensions/traits.hpp>
#include <omnirefl/functional.hpp>
#include <omnirefl/reflected_scope.hpp>
#include <omnirefl/serialization/ryml/core.hpp>
#include <omnirefl/serialization/ryml/impl/shared.hpp>
#include <omnirefl/serialization/ryml/impl/tdm_enum.hpp>
#include <omnirefl/serialization/ryml/impl/tdm_node.hpp>
#include <omnirefl/serialization/ryml/impl/tdm_record.hpp>
#include <omnirefl/serialization/ryml/state.hpp>
#include <omnirefl/traits.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {

// -----------------------------------------------------------------------------
// Mappings requiring reflected scope
// -----------------------------------------------------------------------------

namespace core {
namespace reflected_scope {

/** Maps a string node into a reflected enum value.
 *
```
map_node_to_enum :: (state, node, type<T>) -> with_diagnostics<T>
```
 *
 * @pre node is a string in a valid tree; enum metadata is available in
 * reflected scope. */
template <typename Strategy = mapping::enum_strategy_t<>>
struct map_node_to_enum_t:
    detail::tdm_for_map_node_to_enum<Strategy>,
    omni::ryml::detail::with_depth_exceeded_operator {
  using tdm_provider = detail::tdm_for_map_node_to_enum<Strategy>;
  using omni::ryml::detail::with_depth_exceeded_operator::operator();

  static_assert(mapping::is_enum_strategy<Strategy>::value,
    "map_node_to_enum_t requires mapping::enum_strategy_t");

  /** \ref mapping::enum_strategy_t */
  Strategy strategy OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_strategy;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::is_reflected_enum<T>,
      is_read_state<State>>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, omni::type_t<T>) const
    -> with_diagnostics_t<T, diagnostics>;

// Ad hoc: classes with bases are not aggregates before C++17. These
// constructors preserve field initialization in the older standards.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr map_node_to_enum_t() = default;

  OMNI_CONSTRAINED_TEMPLATE((typename Value = Strategy),
    (std::is_constructible<Strategy, Value &&>::value))
  constexpr map_node_to_enum_t(Value &&value)
    : strategy(std::forward<Value>(value)) {}
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_node_to_enum_t<> map_node_to_enum{};
#else
static constexpr const map_node_to_enum_t<> &map_node_to_enum =
  compat::detail::shared_state<map_node_to_enum_t<>>::value;
#endif

} // namespace reflected_scope
} // namespace core

namespace reflected_scope {

// Node to C++ value.

/** Maps an object node into a reflected record.
 *
```
map_object_node :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
```
 *
 * Members are mapped through state.map_node.
 * Non-object input reports an expected-object diagnostic by default.
 * @pre node belongs to a valid tree, pointers are non-null, and record
 * metadata is available in reflected scope. */
template <typename Strategy = mapping::record_strategy_t<>,
  typename WithNodeCheck = std::true_type>
struct map_object_node_t:
    detail::tdm_for_map_object_node<Strategy, WithNodeCheck>,
    omni::ryml::detail::with_depth_exceeded_operator {
  using tdm_provider =
    detail::tdm_for_map_object_node<Strategy, WithNodeCheck>;
  using omni::ryml::detail::with_depth_exceeded_operator::operator();

  static_assert(mapping::is_record_strategy<Strategy>::value,
    "map_object_node_t requires mapping::record_strategy_t");

  /** \ref mapping::record_strategy_t */
  Strategy strategy OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_strategy;

  /** Disable only when the node is already known to be an object. */
  WithNodeCheck with_node_check OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_with_node_check;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (omni::is_reflected_record<T>::value
      && std::is_move_constructible<T>::value
      && std::is_default_constructible<T>::value
      && is_read_state<State>::value))
  auto operator()(State context, ::ryml::ConstNodeRef node,
    omni::type_t<T>) const -> with_diagnostics_t<T, diagnostics>;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (omni::is_reflected_record<T>::value
      && is_read_state<State>::value))
  auto operator()(State context, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;

// Ad hoc: classes with bases are not aggregates before C++17. These
// constructors preserve field initialization in the older standards.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr map_object_node_t() = default;

  OMNI_CONSTRAINED_TEMPLATE((typename Value = Strategy,
    typename CheckValue = WithNodeCheck),
    (std::is_constructible<Strategy, Value &&>::value
      && std::is_constructible<WithNodeCheck, CheckValue &&>::value))
  constexpr map_object_node_t(Value &&value, CheckValue &&check = {})
    : strategy(std::forward<Value>(value)),
      with_node_check(std::forward<CheckValue>(check)) {}
#endif
};

/** Reflected-record mapping with the shared default strategy and
 * node checking enabled. */
#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_object_node_t<> map_object_node{};
#else
static constexpr const map_object_node_t<> &map_object_node =
  compat::detail::shared_state<map_object_node_t<>>::value;
#endif

/** Maps a string node into a reflected enum, resolving names and aliases.
 *
```
map_node_to_enum :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
```
 *
 * @pre node belongs to a valid tree, pointers are non-null, and enum metadata
 * is available in reflected scope. */
template <typename Strategy = mapping::enum_strategy_t<>,
  typename WithNodeCheck = std::true_type>
struct map_node_to_enum_t:
    detail::tdm_for_map_node_to_enum<Strategy, WithNodeCheck>,
    omni::ryml::detail::with_depth_exceeded_operator {
  using tdm_provider =
    detail::tdm_for_map_node_to_enum<Strategy, WithNodeCheck>;
  using omni::ryml::detail::with_depth_exceeded_operator::operator();

  static_assert(mapping::is_enum_strategy<Strategy>::value,
    "map_node_to_enum_t requires mapping::enum_strategy_t");

  /** \ref mapping::enum_strategy_t */
  Strategy strategy OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_strategy;

  /** Disable only when the node is already known to be a string. */
  WithNodeCheck with_node_check OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_with_node_check;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (omni::is_reflected_enum<T>::value && is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node,
    omni::type_t<T> type) const -> with_diagnostics_t<T>;

  /** (QoL) Mutating adapter for the value-producing overload. */
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (omni::is_reflected_enum<T>::value
      && std::is_assignable<T &, T &&>::value
      && is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate>;

// Ad hoc: classes with bases are not aggregates before C++17. These
// constructors preserve field initialization in the older standards.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr map_node_to_enum_t() = default;

  OMNI_CONSTRAINED_TEMPLATE((typename Value = Strategy,
    typename CheckValue = WithNodeCheck),
    (std::is_constructible<Strategy, Value &&>::value
      && std::is_constructible<WithNodeCheck, CheckValue &&>::value))
  constexpr map_node_to_enum_t(Value &&value, CheckValue &&check = {})
    : strategy(std::forward<Value>(value)),
      with_node_check(std::forward<CheckValue>(check)) {}
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_node_to_enum_t<> map_node_to_enum{};
#else
static constexpr const map_node_to_enum_t<> &map_node_to_enum =
  compat::detail::shared_state<map_node_to_enum_t<>>::value;
#endif

// C++ value to node.

/** Maps selected record fields to object entries with canonical names.
 * Members are mapped through state.map_node.
 *
```
map_record :: (state, const T&) -> node
```
 *
 * @pre Record metadata must be available in reflected scope. */
template <typename Strategy = mapping::record_strategy_t<>>
struct map_record_t:
    detail::tdm_for_map_record<Strategy>,
    omni::ryml::detail::with_depth_exceeded_operator {
  using tdm_provider = detail::tdm_for_map_record<Strategy>;
  using omni::ryml::detail::with_depth_exceeded_operator::operator();

  static_assert(mapping::is_record_strategy<Strategy>::value,
    "map_record_t requires mapping::record_strategy_t");

  /** \ref mapping::record_strategy_t */
  Strategy strategy OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_strategy;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::is_reflected_record<T>,
      is_write_state<State>>::value))
  auto operator()(State context, const T &value) const -> ::ryml::NodeRef;

// Ad hoc: classes with bases are not aggregates before C++17. These
// constructors preserve field initialization in the older standards.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr map_record_t() = default;

  OMNI_CONSTRAINED_TEMPLATE((typename Value = Strategy),
    (std::is_constructible<Strategy, Value &&>::value))
  constexpr map_record_t(Value &&value)
    : strategy(std::forward<Value>(value)) {}
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_record_t<> map_record{};
#else
static constexpr const map_record_t<> &map_record =
  compat::detail::shared_state<map_record_t<>>::value;
#endif

/** Maps a reflected enum value to its declared name as a quoted scalar.
 *
```
map_enum :: (state, T) -> node
```
 *
 * @pre Metadata for T must be available in reflected scope. */
template <typename Strategy = mapping::enum_strategy_t<>>
struct map_enum_t:
    detail::tdm_for_map_enum<Strategy>,
    omni::ryml::detail::with_depth_exceeded_operator {
  using tdm_provider = detail::tdm_for_map_enum<Strategy>;
  using omni::ryml::detail::with_depth_exceeded_operator::operator();

  static_assert(mapping::is_enum_strategy<Strategy>::value,
    "map_enum_t requires mapping::enum_strategy_t");

  /** \ref mapping::enum_strategy_t */
  Strategy strategy OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_strategy;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::is_reflected_enum<T>,
      is_write_state<State>>::value))
  auto operator()(State, T value) const -> ::ryml::NodeRef;

// Ad hoc: classes with bases are not aggregates before C++17. These
// constructors preserve field initialization in the older standards.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr map_enum_t() = default;

  OMNI_CONSTRAINED_TEMPLATE((typename Value = Strategy),
    (std::is_constructible<Strategy, Value &&>::value))
  constexpr map_enum_t(Value &&value)
    : strategy(std::forward<Value>(value)) {}
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_enum_t<> map_enum{};
#else
static constexpr const map_enum_t<> &map_enum =
  compat::detail::shared_state<map_enum_t<>>::value;
#endif

} // namespace reflected_scope

// -----------------------------------------------------------------------------
// Mappings without reflected scope
// -----------------------------------------------------------------------------

/** Non-recursive fundamental type mappings may be used inside custom
 * cp::to_type and cp::from_type implementations. They call neither
 * \ref reflected_scope::mapping_to_type nor
 * \ref reflected_scope::mapping_from_type, avoiding recursive selection of
 * the same customization. Optional mappings below dispatch contained values. */

// Node to C++ value.

/** Checked char-based string-like node-to-type mapping.
 *
```
map_string_node :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
```
 *
 * @details String-view destinations borrow the tree's character storage.
 * The tree and its character storage must remain valid while views are used. */
template <typename WithNodeCheck = std::true_type>
struct map_string_node_t:
    detail::tdm_for_map_string_node<WithNodeCheck>,
    detail::with_depth_exceeded_operator {
  using tdm_provider = detail::tdm_for_map_string_node<WithNodeCheck>;
  using detail::with_depth_exceeded_operator::operator();

  /** Disable only when the node satisfies the destination's shape. */
  WithNodeCheck with_node_check OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_with_node_check;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (traits::is_string_like<T>::value && is_read_state<State>::value))
  auto operator()(State state,
    ::ryml::ConstNodeRef node,
    omni::type_t<T> type) const -> with_diagnostics_t<T>;

  /** (QoL) Mutating adapter for the value-producing overload. */
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (traits::is_string_like<T>::value
      && std::is_assignable<T &, T &&>::value
      && is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate>;

// Ad hoc: classes with bases are not aggregates before C++17. These
// constructors preserve field initialization in the older standards.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr map_string_node_t() = default;

  OMNI_CONSTRAINED_TEMPLATE((typename Value = WithNodeCheck),
    (std::is_constructible<WithNodeCheck, Value &&>::value))
  constexpr map_string_node_t(Value &&value)
    : with_node_check(std::forward<Value>(value)) {}
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_string_node_t<> map_string_node{};
#else
static constexpr const map_string_node_t<> &map_string_node =
  compat::detail::shared_state<map_string_node_t<>>::value;
#endif

/** Checked Boolean node-to-type mapping.
 *
```
map_boolean_node :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
```
 */
template <typename WithNodeCheck = std::true_type>
struct map_boolean_node_t:
    detail::tdm_for_map_boolean_node<WithNodeCheck>,
    detail::with_depth_exceeded_operator {
  using tdm_provider = detail::tdm_for_map_boolean_node<WithNodeCheck>;
  using detail::with_depth_exceeded_operator::operator();

  /** Disable only when the node satisfies the destination's shape. */
  WithNodeCheck with_node_check OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_with_node_check;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_same<bool, compat::remove_cv_t<T>>::value
      && !std::is_const<T>::value && is_read_state<State>::value))
  auto operator()(State state,
    ::ryml::ConstNodeRef node,
    omni::type_t<T> type) const -> with_diagnostics_t<T>;

  /** (QoL) Mutating adapter for the value-producing overload. */
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_same<bool, compat::remove_cv_t<T>>::value
      && !std::is_const<T>::value
      && std::is_assignable<T &, T &&>::value
      && is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate>;

// Ad hoc: classes with bases are not aggregates before C++17. These
// constructors preserve field initialization in the older standards.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr map_boolean_node_t() = default;

  OMNI_CONSTRAINED_TEMPLATE((typename Value = WithNodeCheck),
    (std::is_constructible<WithNodeCheck, Value &&>::value))
  constexpr map_boolean_node_t(Value &&value)
    : with_node_check(std::forward<Value>(value)) {}
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_boolean_node_t<> map_boolean_node{};
#else
static constexpr const map_boolean_node_t<> &map_boolean_node =
  compat::detail::shared_state<map_boolean_node_t<>>::value;
#endif

/** Checked integer node-to-type mapping.
 *
```
map_integer_node :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
```
 */
template <typename WithNodeCheck = std::true_type>
struct map_integer_node_t:
    detail::tdm_for_map_integer_node<WithNodeCheck>,
    detail::with_depth_exceeded_operator {
  using tdm_provider = detail::tdm_for_map_integer_node<WithNodeCheck>;
  using detail::with_depth_exceeded_operator::operator();

  /** Disable only when the node satisfies the destination's shape. */
  WithNodeCheck with_node_check OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_with_node_check;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_integral<T>::value
      && !std::is_same<bool, compat::remove_cv_t<T>>::value
      && is_read_state<State>::value))
  auto operator()(State state,
    ::ryml::ConstNodeRef node,
    omni::type_t<T> type) const -> with_diagnostics_t<T>;

  /** (QoL) Mutating adapter for the value-producing overload. */
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_integral<T>::value
      && !std::is_same<bool, compat::remove_cv_t<T>>::value
      && std::is_assignable<T &, T &&>::value
      && is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate>;

// Ad hoc: classes with bases are not aggregates before C++17. These
// constructors preserve field initialization in the older standards.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr map_integer_node_t() = default;

  OMNI_CONSTRAINED_TEMPLATE((typename Value = WithNodeCheck),
    (std::is_constructible<WithNodeCheck, Value &&>::value))
  constexpr map_integer_node_t(Value &&value)
    : with_node_check(std::forward<Value>(value)) {}
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_integer_node_t<> map_integer_node{};
#else
static constexpr const map_integer_node_t<> &map_integer_node =
  compat::detail::shared_state<map_integer_node_t<>>::value;
#endif

/** Checked floating-point node-to-type mapping.
 *
```
map_number_node :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
```
 */
template <typename WithNodeCheck = std::true_type>
struct map_number_node_t:
    detail::tdm_for_map_number_node<WithNodeCheck>,
    detail::with_depth_exceeded_operator {
  using tdm_provider = detail::tdm_for_map_number_node<WithNodeCheck>;
  using detail::with_depth_exceeded_operator::operator();

  /** Disable only when the node satisfies the destination's shape. */
  WithNodeCheck with_node_check OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_with_node_check;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_floating_point<T>::value && is_read_state<State>::value))
  auto operator()(State state,
    ::ryml::ConstNodeRef node,
    omni::type_t<T> type) const -> with_diagnostics_t<T>;

  /** (QoL) Mutating adapter for the value-producing overload. */
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_floating_point<T>::value
      && std::is_assignable<T &, T &&>::value
      && is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate>;

// Ad hoc: classes with bases are not aggregates before C++17. These
// constructors preserve field initialization in the older standards.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr map_number_node_t() = default;

  OMNI_CONSTRAINED_TEMPLATE((typename Value = WithNodeCheck),
    (std::is_constructible<WithNodeCheck, Value &&>::value))
  constexpr map_number_node_t(Value &&value)
    : with_node_check(std::forward<Value>(value)) {}
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_number_node_t<> map_number_node{};
#else
static constexpr const map_number_node_t<> &map_number_node =
  compat::detail::shared_state<map_number_node_t<>>::value;
#endif

/** Checked arithmetic node-to-type mapping.
 *
```
map_arithmetic_node :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
```
 */
template <typename WithNodeCheck = std::true_type>
struct map_arithmetic_node_t:
    detail::tdm_for_map_arithmetic_node<WithNodeCheck>,
    detail::with_depth_exceeded_operator {
  using tdm_provider = detail::tdm_for_map_arithmetic_node<WithNodeCheck>;
  using detail::with_depth_exceeded_operator::operator();

  // Validate the category required by T, rather than accepting every scalar.
  template <typename T>
  using mapping_type = typename traits::select<
    traits::case_<std::is_same<bool, compat::remove_cv_t<T>>::value,
      map_boolean_node_t<WithNodeCheck>>,
    traits::case_<std::is_integral<T>::value,
      map_integer_node_t<WithNodeCheck>>,
    map_number_node_t<WithNodeCheck>>::type;

  /** Disable only when the node satisfies the destination's shape. */
  WithNodeCheck with_node_check OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_with_node_check;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_arithmetic<T>::value
      && (!std::is_const<T>::value
        || !std::is_same<bool, compat::remove_cv_t<T>>::value)
      && is_read_state<State>::value))
  auto operator()(State state,
    ::ryml::ConstNodeRef node,
    omni::type_t<T> type) const -> with_diagnostics_t<T>;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_arithmetic<T>::value
      && std::is_assignable<T &, T &&>::value
      && is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate>;

// Ad hoc: classes with bases are not aggregates before C++17. These
// constructors preserve field initialization in the older standards.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr map_arithmetic_node_t() = default;

  OMNI_CONSTRAINED_TEMPLATE((typename Value = WithNodeCheck),
    (std::is_constructible<WithNodeCheck, Value &&>::value))
  constexpr map_arithmetic_node_t(Value &&value)
    : with_node_check(std::forward<Value>(value)) {}
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_arithmetic_node_t<> map_arithmetic_node{};
#else
static constexpr const map_arithmetic_node_t<> &map_arithmetic_node =
  compat::detail::shared_state<map_arithmetic_node_t<>>::value;
#endif

/** Maps a node into an optional-like destination.
 * Null resets the destination. Non-null input uses the contained type's
 * registered mapping and is emplaced only on success. */
struct to_optional_like_t: detail::with_depth_exceeded_operator {
  using detail::with_depth_exceeded_operator::operator();

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<traits::is_optional<T>, is_read_state<State>>::value))
  auto operator()(State context, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;
};

/** Maps null or a value node into an optional-like destination.
 * Contained values are mapped through state.map_node.
 *
```
map_value_node :: (state, node, T*) -> with_diagnostics<monostate>
```
 */
template <typename WithNodeCheck = std::true_type>
struct map_value_node_t:
    detail::tdm_for_map_value_node<WithNodeCheck>,
    detail::with_depth_exceeded_operator {
  using tdm_provider = detail::tdm_for_map_value_node<WithNodeCheck>;
  using detail::with_depth_exceeded_operator::operator();

  /** Disable only when the node satisfies the destination's shape. */
  WithNodeCheck with_node_check OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_with_node_check;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (traits::is_optional<T>::value && is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate>;

// Ad hoc: classes with bases are not aggregates before C++17. These
// constructors preserve field initialization in the older standards.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr map_value_node_t() = default;

  OMNI_CONSTRAINED_TEMPLATE((typename Value = WithNodeCheck),
    (std::is_constructible<WithNodeCheck, Value &&>::value))
  constexpr map_value_node_t(Value &&value)
    : with_node_check(std::forward<Value>(value)) {}
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_value_node_t<> map_value_node{};
#else
static constexpr const map_value_node_t<> &map_value_node =
  compat::detail::shared_state<map_value_node_t<>>::value;
#endif

/** Maps array nodes into element ranges and object nodes into key/value ranges.
 *
```
to_range :: (state, node, T*) -> with_diagnostics<monostate>
```
 *
 * Elements are mapped through state.map_node.
 * @pre into is non-null; node is valid and has the destination's shape.
 * Object keys are string-like; range operations use \ref adapt::clear and
 * the insertion adapters. */
struct to_range_t: detail::with_depth_exceeded_operator {
  using detail::with_depth_exceeded_operator::operator();

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<detail::is_mapped_range<T>,
      is_read_state<State>>::value))
  auto operator()(State context, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;

  // TODO: [high] Define the representation of non-string object keys before
  // enabling their default mapping. JSON object names are always strings.
};

/** Maps array nodes into element ranges and object nodes into key/value ranges.
 * Elements are mapped through state.map_node.
 *
```
map_range_node :: (state, node, T*) -> with_diagnostics<monostate>
```
 */
template <typename WithNodeCheck = std::true_type>
struct map_range_node_t:
    detail::tdm_for_map_range_node<WithNodeCheck>,
    detail::with_depth_exceeded_operator {
  using tdm_provider = detail::tdm_for_map_range_node<WithNodeCheck>;
  using detail::with_depth_exceeded_operator::operator();

  /** Disable only when the node satisfies the destination's shape. */
  WithNodeCheck with_node_check OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_with_node_check;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (detail::is_mapped_range<T>::value && is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate>;

// Ad hoc: classes with bases are not aggregates before C++17. These
// constructors preserve field initialization in the older standards.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr map_range_node_t() = default;

  OMNI_CONSTRAINED_TEMPLATE((typename Value = WithNodeCheck),
    (std::is_constructible<WithNodeCheck, Value &&>::value))
  constexpr map_range_node_t(Value &&value)
    : with_node_check(std::forward<Value>(value)) {}
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_range_node_t<> map_range_node{};
#else
static constexpr const map_range_node_t<> &map_range_node =
  compat::detail::shared_state<map_range_node_t<>>::value;
#endif

// C++ value to node.

/** Maps a string-like value to a quoted JSON/YAML scalar.
 * String conversion uses \ref adapt::as_string.
 *
```
map_string_like :: (state, const T&) -> node
```
 */
struct map_string_like_t: detail::with_depth_exceeded_operator {
  using detail::with_depth_exceeded_operator::operator();

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<traits::is_string_like<T>,
      is_write_state<State>>::value))
  auto operator()(State, const T &value) const -> ::ryml::NodeRef;
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_string_like_t map_string_like{};
#else
static constexpr const map_string_like_t &map_string_like =
  compat::detail::shared_state<map_string_like_t>::value;
#endif

/** Maps a Boolean value to a JSON/YAML scalar.
 *
```
map_boolean :: (state, T) -> node
```
 */
struct map_boolean_t: detail::with_depth_exceeded_operator {
  using detail::with_depth_exceeded_operator::operator();

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_same<bool, compat::remove_cv_t<T>>::value
      && is_write_state<State>::value))
  auto operator()(State, const T &value) const -> ::ryml::NodeRef;
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_boolean_t map_boolean{};
#else
static constexpr const map_boolean_t &map_boolean =
  compat::detail::shared_state<map_boolean_t>::value;
#endif

/** Maps an integral value to a JSON/YAML scalar.
 *
```
map_integral :: (state, const T&) -> node
```
 */
struct map_integral_t: detail::with_depth_exceeded_operator {
  using detail::with_depth_exceeded_operator::operator();

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_integral<T>::value
      && !std::is_same<bool, compat::remove_cv_t<T>>::value
      && is_write_state<State>::value))
  auto operator()(State, const T &value) const -> ::ryml::NodeRef;
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_integral_t map_integral{};
#else
static constexpr const map_integral_t &map_integral =
  compat::detail::shared_state<map_integral_t>::value;
#endif

/** Maps a floating-point value to a JSON/YAML scalar.
 * Preserves enough significant digits for round-trip conversion.
 *
```
map_floating_point :: (state, const T&) -> node
```
 */
struct map_floating_point_t: detail::with_depth_exceeded_operator {
  using detail::with_depth_exceeded_operator::operator();

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_floating_point<T>::value && is_write_state<State>::value))
  auto operator()(State, const T &value) const -> ::ryml::NodeRef;
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_floating_point_t map_floating_point{};
#else
static constexpr const map_floating_point_t &map_floating_point =
  compat::detail::shared_state<map_floating_point_t>::value;
#endif

/** Maps an arithmetic value to a Boolean, integer, or real JSON/YAML scalar.
 *
```
map_arithmetic :: (state, const T&) -> node
```
 */
struct map_arithmetic_t: detail::with_depth_exceeded_operator {
  using detail::with_depth_exceeded_operator::operator();

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_arithmetic<T>::value && is_write_state<State>::value))
  auto operator()(State, const T &value) const -> ::ryml::NodeRef;
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_arithmetic_t map_arithmetic{};
#else
static constexpr const map_arithmetic_t &map_arithmetic =
  compat::detail::shared_state<map_arithmetic_t>::value;
#endif

/** Maps an empty optional to null, or maps its contained value to a node.
 * Contained values are mapped through state.map_node.
 *
```
map_optional_like :: (state, const T&) -> node
```
 */
struct map_optional_like_t: detail::with_depth_exceeded_operator {
  using detail::with_depth_exceeded_operator::operator();

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<traits::is_optional<T>, is_write_state<State>>::value))
  auto operator()(State context, const T &value) const -> ::ryml::NodeRef;
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_optional_like_t map_optional_like{};
#else
static constexpr const map_optional_like_t &map_optional_like =
  compat::detail::shared_state<map_optional_like_t>::value;
#endif

/** Maps key/value ranges to objects and element ranges to arrays.
 *
```
map_range :: (state, const T&) -> node
```
 *
 * Elements are mapped through state.map_node; empty optional array elements
 * are omitted. \ref traits::is_key_value selects object or array output.
 * @pre Non-scalar elements are exposed as lvalue references to value_type. */
struct map_range_t: detail::with_depth_exceeded_operator {
  using detail::with_depth_exceeded_operator::operator();

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::ryml::detail::is_mapped_range<const T>,
      is_write_state<State>>::value))
  auto operator()(State context, const T &value) const -> ::ryml::NodeRef;
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_range_t map_range{};
#else
static constexpr const map_range_t &map_range =
  compat::detail::shared_state<map_range_t>::value;
#endif

// -----------------------------------------------------------------------------
// Type mapping customization and lookup
// -----------------------------------------------------------------------------

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
template <typename _T, typename T = compat::remove_cv_t<_T>>
using default_mapping_from_type = typename traits::select<
  traits::case_<traits::is_string_like<T>::value, map_string_like_t>,
  traits::case_<std::is_same<bool, T>::value, map_boolean_t>,
  traits::case_<std::is_integral<T>::value, map_integral_t>,
  traits::case_<std::is_floating_point<T>::value, map_floating_point_t>,
  traits::case_<traits::is_optional<T>::value, map_optional_like_t>,
  traits::case_<omni::ryml::detail::is_mapped_range<const T>::value,
    map_range_t>,
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
 * must be copy-constructible. Registration conflicts and missing mappings
 * are diagnosed before invoking the mapping function.
 *
```
mapping_from_type :: type<T> -> ((state, const T&) -> node)
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

#include <omnirefl/serialization/ryml/impl/record.hpp>
#include <omnirefl/serialization/ryml/impl/enum.hpp>
#include <omnirefl/serialization/ryml/impl/sequence.hpp>
#include <omnirefl/serialization/ryml/impl/type_mapping.hpp>

namespace omni {
namespace ryml {
namespace reflected_scope {

template <typename State, typename T>
OMNI_CPP14_CONSTEXPR auto mapping_to_type_t::operator()(const State &,
  omni::type_t<T> type) const
  OMNI_CPP11_TRAILING_RETURN(
    typename omni::ryml::detail::to_type_cpp11_trailing_return<T>::type) {
  return reflected_scope::mapping_to_type(type);
}

template <typename State, typename T>
OMNI_CPP14_CONSTEXPR auto mapping_from_type_t::operator()(const State &,
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

// TODO: [medium] Consider recovery for missing data members in map_object_node,
// including exposed default member initializers.

// FIXME: [high] Separate optional unwrapping from fundamental mappings:
// these optional-like mappings recursively invoke the type-mapping
// dispatchers for the contained value; they are not non-recursive defaults.
