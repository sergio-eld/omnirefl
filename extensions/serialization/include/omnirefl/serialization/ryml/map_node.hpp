#pragma once

/** Node-to-type mappings.
 * Signature notation: docs/CONVENTIONS.md#functional-composition-notation. */

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/mapping_policy.hpp>
#include <omnirefl/extensions/traits.hpp>
#include <omnirefl/reflected_scope.hpp>
#include <omnirefl/serialization/ryml/core.hpp>
#include <omnirefl/serialization/ryml/impl/tdm_enum.hpp>
#include <omnirefl/serialization/ryml/impl/tdm_node.hpp>
#include <omnirefl/serialization/ryml/impl/tdm_record.hpp>
#include <omnirefl/serialization/ryml/state.hpp>
#include <omnirefl/serialization/ryml/with_diagnostics.hpp>
#include <omnirefl/traits.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {
namespace core {
namespace reflected_scope {

/** Converts a string node to an enum through its declared names and aliases.
 *
 * @pre node is a string in a valid tree; enum metadata is available. */
OMNI_CONSTRAINED_TEMPLATE((typename T,
  typename Policy = mapping::enum_policy_t<>),
  (omni::is_reflected_enum<T>::value
    && mapping::is_enum_policy<Policy>::value))
auto parse_as_enum(::ryml::ConstNodeRef node, const Policy &policy = {})
  -> compat::expected<T, issue>;

} // namespace reflected_scope
} // namespace core

namespace reflected_scope {

/** Maps an object node into a reflected record.
 *
```
map_object_node :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
```
 *
 * Members are mapped through state.select_mapping.
 * Non-object input reports an expected-object diagnostic by default.
 * @pre node belongs to a valid tree, pointers are non-null, and record
 * metadata is available in reflected scope. */
template <typename Policy = mapping::record_policy_t<>,
  typename WithNodeCheck = std::true_type>
struct map_object_node_t final:
    detail::tdm_for_map_object_node<Policy, WithNodeCheck>,
    omni::ryml::detail::with_depth_exceeded_operator {
  using tdm_provider =
    detail::tdm_for_map_object_node<Policy, WithNodeCheck>;
  using omni::ryml::detail::with_depth_exceeded_operator::operator();

  static_assert(mapping::is_record_policy<Policy>::value,
    "map_object_node_t requires mapping::record_policy_t");

  /** \ref mapping::record_policy_t */
  Policy policy OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_policy;

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

  OMNI_CONSTRAINED_TEMPLATE((typename Value = Policy,
    typename CheckValue = WithNodeCheck),
    (std::is_constructible<Policy, Value &&>::value
      && std::is_constructible<WithNodeCheck, CheckValue &&>::value))
  constexpr map_object_node_t(Value &&value, CheckValue &&check = {})
    : policy(std::forward<Value>(value)),
      with_node_check(std::forward<CheckValue>(check)) {}
#endif
};

/** Reflected-record mapping with the shared default policy and
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
template <typename Policy = mapping::enum_policy_t<>,
  typename WithNodeCheck = std::true_type>
struct map_node_to_enum_t final:
    detail::tdm_for_map_node_to_enum<Policy, WithNodeCheck>,
    omni::ryml::detail::with_depth_exceeded_operator {
  using tdm_provider =
    detail::tdm_for_map_node_to_enum<Policy, WithNodeCheck>;
  using omni::ryml::detail::with_depth_exceeded_operator::operator();

  static_assert(mapping::is_enum_policy<Policy>::value,
    "map_node_to_enum_t requires mapping::enum_policy_t");

  /** \ref mapping::enum_policy_t */
  Policy policy OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_policy;

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

  OMNI_CONSTRAINED_TEMPLATE((typename Value = Policy,
    typename CheckValue = WithNodeCheck),
    (std::is_constructible<Policy, Value &&>::value
      && std::is_constructible<WithNodeCheck, CheckValue &&>::value))
  constexpr map_node_to_enum_t(Value &&value, CheckValue &&check = {})
    : policy(std::forward<Value>(value)),
      with_node_check(std::forward<CheckValue>(check)) {}
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_node_to_enum_t<> map_node_to_enum{};
#else
static constexpr const map_node_to_enum_t<> &map_node_to_enum =
  compat::detail::shared_state<map_node_to_enum_t<>>::value;
#endif

} // namespace reflected_scope

// -----------------------------------------------------------------------------
// Mappings without reflected scope
// -----------------------------------------------------------------------------

/** Non-recursive fundamental mappings may be used in custom cp::to_type
 * implementations: they do not call \ref reflected_scope::mapping_to_type,
 * avoiding recursive selection of the customization. Optional and range
 * mappings dispatch their contained values through state.select_mapping. */

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
struct map_arithmetic_node_t final:
    detail::tdm_for_map_arithmetic_node<WithNodeCheck>,
    detail::with_depth_exceeded_operator {
  using tdm_provider = detail::tdm_for_map_arithmetic_node<WithNodeCheck>;
  using detail::with_depth_exceeded_operator::operator();

  // Leaf templates are forward-declared in impl/tdm_node.hpp for rebinding.
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
struct map_integer_node_t final:
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
struct map_number_node_t final:
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
struct map_boolean_node_t final:
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
struct map_string_node_t final:
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

/** Maps a node into an optional-like destination.
 *
```
to_optional_like :: (state, node, T*) -> with_diagnostics<monostate>
```
 *
 * Null resets the destination. Non-null input uses the contained type's
 * registered mapping and is emplaced only on success. */
struct to_optional_like_t final: detail::with_depth_exceeded_operator {
  using detail::with_depth_exceeded_operator::operator();

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<traits::is_optional<T>, is_read_state<State>>::value))
  auto operator()(State context, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;
};

/** Maps null or a value node into an optional-like destination.
 * Contained values are mapped through state.select_mapping.
 *
```
map_value_node :: (state, node, T*) -> with_diagnostics<monostate>
```
 */
template <typename WithNodeCheck = std::true_type>
struct map_value_node_t final:
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
 * Elements are mapped through state.select_mapping.
 * @pre into is non-null; node is valid and has the destination's shape.
 * Object keys are string-like; range operations use \ref adapt::clear and
 * the insertion adapters. */
struct to_range_t final: detail::with_depth_exceeded_operator {
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
 * Elements are mapped through state.select_mapping.
 *
```
map_range_node :: (state, node, T*) -> with_diagnostics<monostate>
```
 */
template <typename WithNodeCheck = std::true_type>
struct map_range_node_t final:
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

} // namespace ryml
} // namespace omni

#include <omnirefl/serialization/ryml/impl/map_node.hpp>
