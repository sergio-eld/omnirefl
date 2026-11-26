#pragma once

/** C++-to-node mappings.
 * Signature notation: docs/CONVENTIONS.md#functional-composition-notation. */

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/mapping_strategy.hpp>
#include <omnirefl/extensions/traits.hpp>
#include <omnirefl/reflected_scope.hpp>
#include <omnirefl/serialization/ryml/core.hpp>
#include <omnirefl/serialization/ryml/impl/tdm_enum.hpp>
#include <omnirefl/serialization/ryml/impl/tdm_record.hpp>
#include <omnirefl/serialization/ryml/impl/tdm_type.hpp>
#include <omnirefl/serialization/ryml/state.hpp>
#include <omnirefl/traits.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {
namespace reflected_scope {

/** Maps selected record fields to object entries with canonical names.
 * Members are mapped through state.map_node.
 * Empty optional fields follow \ref serialize_config_t unless `.keep_nulls`
 * overrides it.
 *
```
map_record :: (state, const T&) -> node
```
 *
 * @pre Record metadata must be available in reflected scope. */
template <typename Strategy = mapping::record_strategy_t<>,
  typename KeepNulls = compat::monostate>
struct map_record_t final:
    detail::tdm_for_map_record<Strategy, KeepNulls>,
    omni::ryml::detail::with_depth_exceeded_operator {
  using tdm_provider = detail::tdm_for_map_record<Strategy, KeepNulls>;
  using omni::ryml::detail::with_depth_exceeded_operator::operator();

  static_assert(mapping::is_record_strategy<Strategy>::value,
    "map_record_t requires mapping::record_strategy_t");
  static_assert(std::is_same<compat::monostate, KeepNulls>::value
      || std::is_same<bool, KeepNulls>::value
      || std::is_same<compat::bool_constant<false>, KeepNulls>::value
      || std::is_same<compat::bool_constant<true>, KeepNulls>::value,
    "map_record_t keep_nulls must be compat::monostate, bool, "
    "or compat::bool_constant");

  /** \ref mapping::record_strategy_t */
  Strategy strategy OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_strategy;

  /** `compat::monostate` follows \ref serialize_config_t; `bool` or
   * `compat::bool_constant` overrides it: true keeps nulls, false omits. */
  KeepNulls keep_nulls OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_keep_nulls;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::is_reflected_record<T>,
      is_write_state<State>>::value))
  auto operator()(State context, const T &value) const -> ::ryml::NodeRef;

// Ad hoc: classes with bases are not aggregates before C++17. These
// constructors preserve field initialization in the older standards.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr map_record_t() : strategy{}, keep_nulls{} {}

  OMNI_CONSTRAINED_TEMPLATE((typename Value = Strategy,
    typename Nulls = KeepNulls),
    (std::is_constructible<Strategy, Value &&>::value
      && std::is_constructible<KeepNulls, Nulls &&>::value))
  constexpr map_record_t(Value &&value, Nulls &&nulls = Nulls{})
    : strategy(std::forward<Value>(value)),
      keep_nulls(std::forward<Nulls>(nulls)) {}
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
struct map_enum_t final:
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

/** Non-recursive fundamental mappings may be used in custom cp::from_type
 * implementations: they do not call \ref reflected_scope::mapping_from_type,
 * avoiding recursive selection of the customization. Optional and range
 * mappings dispatch their contained values through state.map_node. */

/** Maps an arithmetic value to a Boolean, integer, or real JSON/YAML scalar.
 *
```
map_arithmetic :: (state, const T&) -> node
```
 */
struct map_arithmetic_t final: detail::with_depth_exceeded_operator {
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

/** Maps an integral value to a JSON/YAML scalar.
 *
```
map_integral :: (state, const T&) -> node
```
 */
struct map_integral_t final: detail::with_depth_exceeded_operator {
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
struct map_floating_point_t final: detail::with_depth_exceeded_operator {
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

/** Maps a Boolean value to a JSON/YAML scalar.
 *
```
map_boolean :: (state, const T&) -> node
```
 */
struct map_boolean_t final: detail::with_depth_exceeded_operator {
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

/** Maps a string-like value to a quoted JSON/YAML scalar.
 * String conversion uses \ref adapt::as_string.
 *
```
map_string_like :: (state, const T&) -> node
```
 */
struct map_string_like_t final: detail::with_depth_exceeded_operator {
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

/** Maps an optional's value through state.map_node, or writes null.
 * Enclosing record/range mappings decide whether empty optionals are omitted.
 *
```
map_optional_like :: (state, const T&) -> node
```
 */
struct map_optional_like_t final: detail::with_depth_exceeded_operator {
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
 * Elements are mapped through state.map_node. Null omission follows
 * \ref serialize_config_t unless `.keep_nulls` overrides it.
 * This policy covers immediate elements; nested records/ranges own theirs.
 * \ref traits::is_key_value selects object or array output.
 * @pre Non-scalar elements are exposed as lvalue references to value_type. */
template <typename KeepNulls = compat::monostate>
struct map_range_t final:
    detail::tdm_for_map_range<KeepNulls>,
    detail::with_depth_exceeded_operator {
  using tdm_provider = detail::tdm_for_map_range<KeepNulls>;
  using detail::with_depth_exceeded_operator::operator();

  static_assert(std::is_same<compat::monostate, KeepNulls>::value
      || std::is_same<bool, KeepNulls>::value
      || std::is_same<compat::bool_constant<false>, KeepNulls>::value
      || std::is_same<compat::bool_constant<true>, KeepNulls>::value,
    "map_range_t keep_nulls must be compat::monostate, bool, "
    "or compat::bool_constant");

  /** `compat::monostate` follows \ref serialize_config_t; `bool` or
   * `compat::bool_constant` overrides it: true keeps nulls, false omits. */
  KeepNulls keep_nulls OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_keep_nulls;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::ryml::detail::is_mapped_range<const T>,
      is_write_state<State>>::value))
  auto operator()(State context, const T &value) const -> ::ryml::NodeRef;

// Ad hoc: aggregate bases need constructors before C++17.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr map_range_t() : keep_nulls{} {}

  OMNI_CONSTRAINED_TEMPLATE((typename Value = KeepNulls),
    (std::is_constructible<KeepNulls, Value &&>::value))
  constexpr map_range_t(Value &&value)
    : keep_nulls(std::forward<Value>(value)) {}
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_range_t<> map_range{};
#else
static constexpr const map_range_t<> &map_range =
  compat::detail::shared_state<map_range_t<>>::value;
#endif

} // namespace ryml
} // namespace omni

#include <omnirefl/serialization/ryml/impl/map_type.hpp>

// TODO: [medium] Add explicit-null read/write mappings for nullptr_t and
// compat::monostate in a separate PR, independent of optional omission.
// Consider nullopt_t only for writing: optional<nullopt_t> is ill-formed.

// TODO: [medium] Revisit parent omission vs custom empty-optional mappings.
// A custom sequence compositor can emit values such as "unavailable"; consider
// an observable trait only if needed, since that would be an ad hoc.

// REFACTORME: [medium] Consider sharing `.keep_nulls` with schema rendering,
// including in-place overrides for RPC return types. Input field requirements
// need a separate policy.
