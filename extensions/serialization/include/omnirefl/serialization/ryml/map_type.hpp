#pragma once

/** C++-to-node mappings.
 * Signature notation: docs/CONVENTIONS.md#functional-composition-notation. */

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/mapping_policy.hpp>
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
 * Members are mapped through state.select_mapping.
 * Empty optional fields follow \ref serialize_config_t unless
 * `.policy.keep_nulls` overrides it.
 *
```
map_record :: (state, const T&) -> node
```
 *
 * @pre Record metadata must be available in reflected scope. */
template <typename Policy = mapping::record_policy_t<>>
struct map_record_t final:
    detail::tdm_for_map_record<Policy>,
    omni::ryml::detail::with_depth_exceeded_operator {
  using tdm_provider = detail::tdm_for_map_record<Policy>;
  using omni::ryml::detail::with_depth_exceeded_operator::operator();

  static_assert(mapping::is_record_policy<Policy>::value,
    "map_record_t requires mapping::record_policy_t");

  /** \ref mapping::record_policy_t */
  Policy policy OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_policy;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::is_reflected_record<T>,
      is_write_state<State>>::value))
  auto operator()(State context, const T &value) const -> ::ryml::NodeRef;

// Ad hoc: classes with bases are not aggregates before C++17. These
// constructors preserve field initialization in the older standards.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr map_record_t() = default;

  OMNI_CONSTRAINED_TEMPLATE((typename Value = Policy),
    (std::is_constructible<Policy, Value &&>::value))
  constexpr map_record_t(Value &&value)
    : policy(std::forward<Value>(value)) {}
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
template <typename Policy = mapping::enum_policy_t<>>
struct map_enum_t final:
    detail::tdm_for_map_enum<Policy>,
    omni::ryml::detail::with_depth_exceeded_operator {
  using tdm_provider = detail::tdm_for_map_enum<Policy>;
  using omni::ryml::detail::with_depth_exceeded_operator::operator();

  static_assert(mapping::is_enum_policy<Policy>::value,
    "map_enum_t requires mapping::enum_policy_t");

  /** \ref mapping::enum_policy_t */
  Policy policy OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_policy;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::is_reflected_enum<T>,
      is_write_state<State>>::value))
  auto operator()(State, T value) const -> ::ryml::NodeRef;

// Ad hoc: classes with bases are not aggregates before C++17. These
// constructors preserve field initialization in the older standards.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr map_enum_t() = default;

  OMNI_CONSTRAINED_TEMPLATE((typename Value = Policy),
    (std::is_constructible<Policy, Value &&>::value))
  constexpr map_enum_t(Value &&value)
    : policy(std::forward<Value>(value)) {}
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
 * mappings dispatch their contained values through state.select_mapping. */

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

/** Maps an optional's value through state.select_mapping, or writes null.
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
 * Elements are mapped through state.select_mapping. Null omission follows
 * \ref serialize_config_t unless `.policy.keep_nulls` overrides it.
 * This policy covers immediate elements; nested records/ranges own theirs.
 * \ref traits::is_key_value selects object or array output.
 * @pre Non-scalar elements are exposed as lvalue references to value_type. */
template <typename Policy = mapping::range_policy_t<>>
struct map_range_t final:
    detail::tdm_for_map_range<Policy>,
    detail::with_depth_exceeded_operator {
  using tdm_provider = detail::tdm_for_map_range<Policy>;
  using detail::with_depth_exceeded_operator::operator();

  static_assert(mapping::is_range_policy<Policy>::value,
    "map_range_t requires mapping::range_policy_t");

  /** \ref mapping::range_policy_t */
  Policy policy OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_policy;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::ryml::detail::is_mapped_range<const T>,
      is_write_state<State>>::value))
  auto operator()(State context, const T &value) const -> ::ryml::NodeRef;

// Ad hoc: aggregate bases need constructors before C++17.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr map_range_t() = default;

  OMNI_CONSTRAINED_TEMPLATE((typename Value = Policy),
    (std::is_constructible<Policy, Value &&>::value))
  constexpr map_range_t(Value &&value)
    : policy(std::forward<Value>(value)) {}
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
