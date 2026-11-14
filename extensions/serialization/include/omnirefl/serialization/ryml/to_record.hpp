#pragma once

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/functional.hpp>
#include <omnirefl/reflected_scope.hpp>
#include <omnirefl/serialization/ryml/detail.hpp>

#include <ryml.hpp>

#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {
namespace reflected_scope {

/** Deserialize a node into a reflected record's accessible non-static data
 * members.
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
 * with_name_by_alias returns a new mapper storing the supplied callable's
 * concrete type.
 * Returned canonical-name views must remain valid throughout the mapping call.
 */
template <typename NameByAlias = fn::default_value<
  compat::optional<compat::string_view>>>
struct to_record_t {
  template <typename T>
  static constexpr bool supports() {
    return omni::is_reflected_record<T>::value;
  }

  template <typename T,
    typename Config,
    compat::enable_if_t<omni::is_reflected_record<T>::value, int> = 0>
  auto operator()(::ryml::ConstNodeRef node,
    T *into,
    mapping_state<Config> context) const
    -> with_diagnostics<compat::monostate, diagnostics>;

  NameByAlias name_by_alias;

  /** QoL for overriding the alias resolver inline, in a single expression.
   * Copy or move f into a new mapper specialized for its concrete type.
   *
   * Resolves alias text to an optional canonical name:
   * f :: compat::string_view -> compat::optional<compat::string_view>
   *
   * An empty result means the input is not a registered alias. Returned views
   * must remain valid throughout the mapping call.
   *
   * The callable must be copy-constructible and invocable through a const
   * reference, returning the same result for the same alias.
   */
  template <typename F>
  constexpr auto with_name_by_alias(F &&f) const
    -> to_record_t<compat::decay_t<F>> {
    static_assert(std::is_copy_constructible<compat::decay_t<F>>::value,
      "with_name_by_alias requires a copy-constructible callable");

    static_assert(detail::is_alias_resolver<const compat::decay_t<F>>::value,
      "Invalid name_by_alias callable; expected: "
      "name_by_alias :: compat::string_view -> optional<compat::string_view>");

    return {/*name_by_alias=*/std::forward<F>(f)};
  }

  // TODO: [medium] Add with_validate_fields for configurable per-field
  // validation, following the alias compositor's functional update pattern.
  // Explore T -> expected<T, error> and T -> with_diagnostics<T> for decoded
  // values, such as email strings or numeric ranges, before field assignment.
  // Retain the source node's diagnostic path and apply normal error policies.
  // This should not require exception-based invariant-enforcing constructors
  // or make mapping aware of arbitrary factories with multiple parameters.
  // Define the responsibility boundary with cp::to_type, which already returns
  // mapping diagnostics, before considering a separate cp::validate for types.
};

/** Default record deserializer without aliases. */
constexpr to_record_t<> to_record{};

} // namespace reflected_scope
} // namespace ryml
} // namespace omni
