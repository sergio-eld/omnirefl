#pragma once

/** Built-in reflected record mappings. */

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

/** Maps an object node into a reflected record's selected instance fields.
 *
```
map_object_node :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
```
 *
 * Does not call \ref mapping_to_type for the record. Nested members may use
 * state.map_node, which defaults to that lookup.
 * Traversal follows the record schema over the node, applying config's
 * duplicate, unknown-member, and diagnostic-limit policies.
 * Aggregatable records with all instance fields selected are constructed
 * from mapped values. Other cases assign values through field bindings and
 * set_value.
 *
 * A canonical name takes precedence over its aliases, even when its value is
 * invalid. Duplicate policies select among canonical occurrences when present,
 * otherwise among aliases. Diagnostics use the original node keys.
 *
 * strategy supplies field selection, aliases, field validators, and record
 * finalization; tdm_strategy returns an adjusted mapping. Static members are
 * always excluded from deserialization. Filtered fields are not required;
 * their document keys follow the unknown-field policy. The type<T> overload
 * requires a default-constructible record; T* accepts an existing record.
 * Validators return expected<T, issue or range<issue>> or with_diagnostics<T>.
 * They preserve values and run before assignment; finalization runs after
 * successful record construction, including for nested records.
 * Nonidentity finalization through T* requires a move-constructible record
 * assignable from its returned value. Identity finalization adds no such
 * requirement. Literal validator keys need typed or SFINAE-friendly callbacks;
 * OMNI_META_STRING keys select the field before probing generic callbacks.
 * @pre The destination pointer must be non-null and node must belong to a
 * valid tree. Non-object input produces an expected-object diagnostic.
 * Record metadata must be available in reflected scope.
 */
template <typename Strategy = mapping::record_strategy_t<>>
struct map_object_node_t {
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

  Strategy strategy OMNI_CPP14_DEFAULT_INIT();

  /** (QoL) Functional update of the shared mapping strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, const Strategy &>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  constexpr auto tdm_strategy(F &&f) const &
    -> map_object_node_t<compat::decay_t<Result>> {
    return {compat::invoke(std::forward<F>(f), this->strategy)};
  }

  /** (QoL) Functional update consuming the bound strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, Strategy &&>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_strategy(F &&f) &&
    -> map_object_node_t<compat::decay_t<Result>> {
    return {compat::invoke(std::forward<F>(f),
      std::move(this->strategy))};
  }

};

/** Reflected-record mapping with the shared default strategy.
 * tdm_strategy binds a configured strategy; non-object input is rejected. */
#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_object_node_t<> map_object_node{};
#else
static constexpr const map_object_node_t<> &map_object_node =
  compat::detail::shared_state<map_object_node_t<>>::value;
#endif

/** Maps selected record fields to object entries with canonical names.
 * The default strategy excludes static and functional members.
 * Does not call \ref mapping_from_type for the record. Nested members may use
 * state.map_node, which defaults to that lookup.
 *
```
map_record :: (state, const T&, node) -> void
```
 *
 * @pre Record metadata must be available in reflected scope. */
template <typename Strategy = mapping::record_strategy_t<>>
struct map_record_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::is_reflected_record<T>,
      is_write_state<State>>::value))
  auto operator()(State context, const T &value, ::ryml::NodeRef node) const
    -> void;

  Strategy strategy OMNI_CPP14_DEFAULT_INIT();

  /** (QoL) Functional update of the shared mapping strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, const Strategy &>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  constexpr auto tdm_strategy(F &&f) const &
    -> map_record_t<compat::decay_t<Result>> {
    return {compat::invoke(std::forward<F>(f), this->strategy)};
  }

  /** (QoL) Functional update consuming the bound strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, Strategy &&>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_strategy(F &&f) &&
    -> map_record_t<compat::decay_t<Result>> {
    return {compat::invoke(std::forward<F>(f),
      std::move(this->strategy))};
  }

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
