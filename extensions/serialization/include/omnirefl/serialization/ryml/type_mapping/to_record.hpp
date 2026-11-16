#pragma once

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/functional.hpp>
#include <omnirefl/reflected_scope.hpp>
#include <omnirefl/serialization/ryml/impl/shared.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {
namespace reflected_scope {

/** Deserialize a node into a reflected record's accessible non-static data
 * members. Direct calls bypass mapping_to_type selection for the record; member
 * mappings still use mapping_to_type dispatch.
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
 * tdm_name_by_alias transforms the resolver into a new mapper.
 * Returned canonical-name views must remain valid throughout the mapping call.
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

  /** (QoL) Transform data member `.name_by_alias` inplace in an expression.
   * Return a new mapper with f(current resolver); the source is unchanged.
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

  // TODO: [medium] Add with_validate_fields for configurable per-field
  // validation, following the alias compositor's functional update pattern.
  // Explore T -> expected<T, error> and T -> with_diagnostics_t<T> for decoded
  // values, such as email strings or numeric ranges, before field assignment.
  // Retain the source node's diagnostic path and apply normal error policies.
  // This should not require exception-based invariant-enforcing constructors
  // or make mapping aware of arbitrary factories with multiple parameters.
  // Define the responsibility boundary with cp::to_type, which already returns
  // mapping diagnostics, before considering a separate cp::validate for types.
};

constexpr to_record_t<> to_record{};

/** Write accessible non-static data members as mapping entries. Direct calls
 * bypass mapping_from_type selection for the record; member mappings still use
 * mapping_from_type dispatch. */
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
