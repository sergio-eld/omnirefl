#pragma once

/** Reflected enum mappings and node-to-type composition. */

#include <omnirefl/serialization/ryml/impl/tdm_enum.hpp>
#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/mapping_strategy.hpp>
#include <omnirefl/functional.hpp>
#include <omnirefl/reflected_scope.hpp>
#include <omnirefl/serialization/ryml/core.hpp>
#include <omnirefl/serialization/ryml/impl/shared.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {
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

/** Maps a reflected enum value to its declared name as a quoted scalar.
 *
```
map_enum :: (state, T, node) -> void
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
  auto operator()(State, T value, ::ryml::NodeRef node) const -> void;

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
} // namespace ryml
} // namespace omni

#include <omnirefl/serialization/ryml/impl/enum.hpp>
