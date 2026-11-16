#pragma once

/** Reflected record mappings and node-to-type composition. */

#include <omnirefl/extensions/compat.hpp>
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

/** Maps an object node into a reflected record's accessible non-static fields.
 * Does not call \ref mapping_to_type for the record; member mappings still
 * use it.
 * Traversal follows the record schema over the node, applying config's
 * duplicate, unknown-member, and diagnostic-limit policies.
 *
 * A canonical name takes precedence over its aliases, even when its value is
 * invalid. Duplicate policies select among canonical occurrences when present,
 * otherwise among aliases. Diagnostics use the original node keys.
 *
 * NameByAlias maps alias text to an optional canonical name.
 * An empty result means the input is not a registered alias. The default
 * resolver accepts no aliases.
 * Returned canonical-name views must remain valid throughout the mapping call.
 * @pre The destination pointer must be non-null and node must belong to a
 * valid tree and be an object. Record metadata must be available in reflected
 * scope; node shape is checked only by a debug assertion.
 */
template <typename NameByAlias = fn::default_value<
  compat::optional<compat::string_view>>>
struct to_record_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::is_reflected_record<T>,
      is_read_state<State>>::value))
  auto operator()(::ryml::ConstNodeRef node, T *into, State context) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;

  NameByAlias name_by_alias;

#if 0
  // TODO: [high] Restore this functional update on the model interface;
  // alias configuration does not belong to the mapping implementation.
  /** (QoL) Transforms data member `.name_by_alias` inplace in an expression.
   * Returns a new mapping function with f(current resolver).
   * The returned resolver must be copy-constructible and const-invocable as
   * string_view -> optional<string_view>, with stable results for each input.
   * Returned views must remain valid throughout the mapping call.
   */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::conjunction<
      std::is_copy_constructible<compat::decay_t<
        compat::invoke_result_t<F &&, const NameByAlias &>>>,
      omni::ryml::detail::is_alias_resolver<const compat::decay_t<
        compat::invoke_result_t<F &&, const NameByAlias &>>>>::value))
  constexpr auto tdm_name_by_alias(F &&f) const
    -> to_record_t<compat::decay_t<
      compat::invoke_result_t<F &&, const NameByAlias &>>> {
    return {/*name_by_alias=*/compat::invoke(
      std::forward<F>(f), this->name_by_alias)};
  }
#endif

  // TODO: [medium] Define model-configured per-field validation for to_record.
  // Alias and validation configuration belong to the model interface.
  // Explore T -> expected<T, error> and T -> with_diagnostics_t<T> for decoded
  // values, such as email strings or numeric ranges, before field assignment.
  // Retain the source node's diagnostic path and apply normal error policies.
  // This should not require exception-based invariant-enforcing constructors
  // or make mapping aware of arbitrary factories with multiple parameters.
  // Define the responsibility boundary with cp::to_type, which already returns
  // mapping diagnostics, before considering a separate cp::validate for types.
};

/** Creates a reflected-record node-to-type mapping function.
 * Checks for object input before traversing the record.
 * Does not call \ref mapping_to_type for the record; member mappings still
 * use it.
 *
```text
to_record :: type<T> ->
  (node, T*, state) -> with_diagnostics<monostate>
```
 */
template <typename T>
constexpr auto to_record(omni::type_t<T>)
  -> decltype(compose::with_node_check<core::expect_object_node>(
    to_record_t<>{})) {
  return compose::with_node_check<core::expect_object_node>(to_record_t<>{});
}

/** Creates a record mapping from a node-only expected function.
 * Object-node validation runs before f. Error is issue or json::issue_code.
 *
```text
to_record :: (node -> expected<T, Error>) -> {
  (node, type<T>, state) -> with_diagnostics<T>,
  (node, T*, state)      -> with_diagnostics<monostate>
}
```
 */
OMNI_CONSTRAINED_TEMPLATE((typename Function),
  (compat::is_invocable<const compat::decay_t<Function> &,
    ::ryml::ConstNodeRef>::value))
constexpr auto to_record(Function &&f)
  -> decltype(compose::with_mutating_node_to_type_overload(
    compose::with_node_check<core::expect_object_node>(
      compose::from_expected_node_function(std::forward<Function>(f))))) {
  return compose::with_mutating_node_to_type_overload(
    compose::with_node_check<core::expect_object_node>(
      compose::from_expected_node_function(std::forward<Function>(f))));
}

/** Maps accessible non-static record fields to object entries.
 * Does not call \ref mapping_from_type for the record; member mappings still
 * use it.
 *
```text
from_record :: (const T&, node, state) -> void
```
 *
 * @pre Record metadata must be available in reflected scope. */
struct from_record_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::is_reflected_record<T>,
      is_write_state<State>>::value))
  auto operator()(const T &value, ::ryml::NodeRef node, State context) const
    -> void;
};

constexpr from_record_t from_record{};

} // namespace reflected_scope
} // namespace ryml
} // namespace omni

#include <omnirefl/serialization/ryml/impl/record.hpp>

// TODO: [medium] Consider recovery for missing data members in to_record,
// including exposed default member initializers.
