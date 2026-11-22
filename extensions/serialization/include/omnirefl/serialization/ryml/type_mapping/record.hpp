#pragma once

/** Built-in reflected record mappings. */

#include <omnirefl/serialization/ryml/impl/tdm_record.hpp>
#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/mapping_strategy.hpp>
#include <omnirefl/functional.hpp>
#include <omnirefl/reflected_scope.hpp>
#include <omnirefl/serialization/ryml/state.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {
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
 * Members are mapped through state.map_node.
 * Non-object input reports an expected-object diagnostic by default.
 * @pre node belongs to a valid tree, pointers are non-null, and record
 * metadata is available in reflected scope. */
template <typename Strategy = mapping::record_strategy_t<>,
  typename WithNodeCheck = std::true_type>
struct map_object_node_t:
    detail::tdm_for_map_object_node<Strategy, WithNodeCheck> {
  using tdm_provider =
    detail::tdm_for_map_object_node<Strategy, WithNodeCheck>;

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

/** Maps selected record fields to object entries with canonical names.
 * Members are mapped through state.map_node.
 *
```
map_record :: (state, const T&, node) -> void
```
 *
 * @pre Record metadata must be available in reflected scope. */
template <typename Strategy = mapping::record_strategy_t<>>
struct map_record_t:
    detail::tdm_for_map_record<Strategy> {
  using tdm_provider = detail::tdm_for_map_record<Strategy>;

  static_assert(mapping::is_record_strategy<Strategy>::value,
    "map_record_t requires mapping::record_strategy_t");

  /** \ref mapping::record_strategy_t */
  Strategy strategy OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_strategy;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::is_reflected_record<T>,
      is_write_state<State>>::value))
  auto operator()(State context, const T &value, ::ryml::NodeRef node) const
    -> void;

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

} // namespace reflected_scope
} // namespace ryml
} // namespace omni

#include <omnirefl/serialization/ryml/impl/record.hpp>

// TODO: [medium] Consider recovery for missing data members in map_object_node,
// including exposed default member initializers.
