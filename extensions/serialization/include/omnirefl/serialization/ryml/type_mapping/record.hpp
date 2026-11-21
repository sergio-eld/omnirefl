#pragma once

/** Built-in reflected record mappings. */

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
 * Does not call \ref mapping_to_type for the record. Nested members use
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
 * NameByAlias maps alias text to an optional canonical name.
 * An empty result means the input is not a registered alias. The default
 * resolver accepts no aliases.
 * Returned canonical-name views must remain valid throughout the mapping call.
 * @pre The destination pointer must be non-null and node must belong to a
 * valid tree and be an object. Record metadata must be available in reflected
 * scope; node shape is checked only by a debug assertion.
 */
template <typename NameByAlias = omni::fn::default_value<
  compat::optional<compat::string_view>>>
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

  NameByAlias name_by_alias;

#if 0
  // TODO: [high] Revisit expression-based configuration of record aliases.
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

  // TODO: [medium] Define per-field validation for the built-in record mapping.
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
 * Does not call \ref mapping_to_type for the record. Nested members use
 * state.map_node, which defaults to that lookup.
 *
```
default_to_record :: type<T> -> {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
```
 */
template <typename T>
constexpr auto default_to_record(omni::type_t<T>)
  -> compose::detail::with_node_check_t<core::expect_object_node,
    to_record_t<>> {
  return {};
}

/** Maps accessible non-static record fields to object entries.
 * Does not call \ref mapping_from_type for the record. Nested members use
 * state.map_node, which defaults to that lookup.
 *
```
from_record :: (state, const T&, node) -> void
```
 *
 * @pre Record metadata must be available in reflected scope. */
struct from_record_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::is_reflected_record<T>,
      is_write_state<State>>::value))
  auto operator()(State context, const T &value, ::ryml::NodeRef node) const
    -> void;

};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr from_record_t from_record{};
#else
static constexpr const from_record_t &from_record =
  compat::detail::shared_state<from_record_t>::value;
#endif

} // namespace reflected_scope
} // namespace ryml
} // namespace omni

#include <omnirefl/serialization/ryml/impl/record.hpp>

// TODO: [medium] Consider recovery for missing data members in to_record,
// including exposed default member initializers.
