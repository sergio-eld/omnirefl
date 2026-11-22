#pragma once

/** Built-in reflected record mappings. */

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/mapping_strategy.hpp>
#include <omnirefl/functional.hpp>
#include <omnirefl/reflected_scope.hpp>
#include <omnirefl/serialization/ryml/compose.hpp>
#include <omnirefl/serialization/ryml/impl/shared.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {
namespace reflected_scope {

/** Maps an object node into a reflected record's selected instance fields.
 * Does not call \ref mapping_to_type for the record. Nested members may use
 * state.map_node, which defaults to that lookup.
 * Traversal follows the record schema over the node, applying config's
 * duplicate, unknown-member, and diagnostic-limit policies.
 * Aggregatable records are constructed from mapped field values. Other
 * records receive those values through their field bindings and set_value.
 *
 * A canonical name takes precedence over its aliases, even when its value is
 * invalid. Duplicate policies select among canonical occurrences when present,
 * otherwise among aliases. Diagnostics use the original node keys.
 *
 * strategy supplies field selection, aliases, field validators, and record
 * finalization; tdm_strategy returns an adjusted mapping. Static members are
 * always excluded from deserialization. Filtered fields are not required;
 * their document keys follow the unknown-field policy. Constructing a value
 * with omitted fields requires a default-constructible record.
 * Validators return expected<T, issue or range<issue>> or with_diagnostics<T>.
 * They preserve values and run before assignment; finalization runs after
 * successful record construction, including for nested records.
 * @pre The destination pointer must be non-null and node must belong to a
 * valid tree and be an object. Record metadata must be available in reflected
 * scope; node shape is checked only by a debug assertion.
 */
template <typename Strategy = mapping::record_strategy_t<>>
struct to_record_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::is_reflected_record<T>,
      compat::disjunction<std::is_default_constructible<T>,
        omni::refl::is_aggregatable<T>>, is_read_state<State>>::value))
  auto operator()(State context, ::ryml::ConstNodeRef node,
    omni::type_t<T>) const -> with_diagnostics_t<T, diagnostics>;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::is_reflected_record<T>,
      is_read_state<State>>::value))
  auto operator()(State context, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;

  Strategy strategy OMNI_CPP14_DEFAULT_INIT();

  /** (QoL) Functional update of the shared mapping strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, const Strategy &>::value))
  constexpr auto tdm_strategy(F &&f) const &
    -> to_record_t<compat::decay_t<
      compat::invoke_result_t<F &&, const Strategy &>>> {
    return {compat::invoke(std::forward<F>(f), this->strategy)};
  }

  /** (QoL) Functional update consuming the bound strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, Strategy &&>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_strategy(F &&f) &&
    -> to_record_t<compat::decay_t<
      compat::invoke_result_t<F &&, Strategy &&>>> {
    return {compat::invoke(std::forward<F>(f),
      std::move(this->strategy))};
  }

};

/** Reflected-record mapping with the shared default strategy.
 * tdm_strategy binds a configured strategy; the node must be an object. */
#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr to_record_t<> to_record{};
#else
static constexpr const to_record_t<> &to_record =
  compat::detail::shared_state<to_record_t<>>::value;
#endif

/** Creates a reflected-record node-to-type mapping function.
 * Checks for object input before traversing the record.
 * Does not call \ref mapping_to_type for the record. Nested members may use
 * state.map_node, which defaults to that lookup.
 *
```
default_to_record :: (type<T>, strategy = record_strategy) -> {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
```
 */
template <typename T, typename Strategy = mapping::record_strategy_t<>>
constexpr auto default_to_record(omni::type_t<T>, Strategy strategy = {})
  -> compose::detail::with_node_check_t<core::expect_object_node,
    to_record_t<Strategy>> {
  return {/*lifted_mapping=*/{std::move(strategy)}};
}

/** Maps selected record fields to object entries with canonical names.
 * The default strategy excludes static and functional members.
 * Does not call \ref mapping_from_type for the record. Nested members may use
 * state.map_node, which defaults to that lookup.
 *
```
from_record :: (state, const T&, node) -> void
```
 *
 * @pre Record metadata must be available in reflected scope. */
template <typename Strategy = mapping::record_strategy_t<>>
struct from_record_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::is_reflected_record<T>,
      is_write_state<State>>::value))
  auto operator()(State context, const T &value, ::ryml::NodeRef node) const
    -> void;

  Strategy strategy OMNI_CPP14_DEFAULT_INIT();

  /** (QoL) Functional update of the shared mapping strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, const Strategy &>::value))
  constexpr auto tdm_strategy(F &&f) const &
    -> from_record_t<compat::decay_t<
      compat::invoke_result_t<F &&, const Strategy &>>> {
    return {compat::invoke(std::forward<F>(f), this->strategy)};
  }

  /** (QoL) Functional update consuming the bound strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, Strategy &&>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_strategy(F &&f) &&
    -> from_record_t<compat::decay_t<
      compat::invoke_result_t<F &&, Strategy &&>>> {
    return {compat::invoke(std::forward<F>(f),
      std::move(this->strategy))};
  }

};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr from_record_t<> from_record{};
#else
static constexpr const from_record_t<> &from_record =
  compat::detail::shared_state<from_record_t<>>::value;
#endif

} // namespace reflected_scope
} // namespace ryml
} // namespace omni

#include <omnirefl/serialization/ryml/impl/record.hpp>

// TODO: [medium] Consider recovery for missing data members in to_record,
// including exposed default member initializers.
