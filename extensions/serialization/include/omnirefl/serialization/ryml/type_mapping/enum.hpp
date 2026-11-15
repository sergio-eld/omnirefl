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

/** Deserialize a string-valued node into a reflected enum value without
 * recursive mapping_to_type dispatch. Direct calls bypass mapping_to_type
 * selection.
 * Declared enumerator names take precedence over aliases. Invalid node kinds
 * and unknown names produce diagnostics containing the original node value.
 *
 * ValueByAlias maps alias text to an optional canonical value text.
 * An empty result means the input is not a registered alias. The default
 * resolver accepts no aliases.
 * with_value_by_alias returns a new mapper storing the supplied callable's
 * concrete type.
 * Returned canonical-value views must remain valid throughout the mapping call.
 */
template <typename ValueByAlias = fn::default_value<
  compat::optional<compat::string_view>>>
struct to_enum_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<is_read_state<State>,
      omni::is_reflected_enum<T>>::value))
  auto operator()(::ryml::ConstNodeRef node, T *into, State context) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;

  ValueByAlias value_by_alias;

  /** (QoL) Override the alias resolver inline, in a single expression.
   * Copy or move f into a new mapper specialized for its concrete type.
   *
   * Resolves alias text to an optional canonical value text:
   * f :: compat::string_view -> compat::optional<compat::string_view>
   *
   * An empty result means the input is not a registered alias. Returned views
   * must remain valid throughout the mapping call.
   *
   * The callable must be copy-constructible and invocable through a const
   * reference, returning the same result for the same alias.
   */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::conjunction<std::is_copy_constructible<compat::decay_t<F>>,
      std::is_constructible<compat::decay_t<F>, F &&>,
      omni::ryml::detail::is_alias_resolver<const compat::decay_t<F>>>::value))
  constexpr auto with_value_by_alias(F &&f) const
    -> to_enum_t<compat::decay_t<F>> {
    return {/*value_by_alias=*/std::forward<F>(f)};
  }
};

constexpr to_enum_t<> to_enum{};

/** Write a declared enumerator name as a quoted scalar without recursive
 * mapping_from_type dispatch. Direct calls bypass mapping_from_type selection. */
struct from_enum_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<is_write_state<State>,
      omni::is_reflected_enum<T>>::value))
  auto operator()(T value, ::ryml::NodeRef node, State) const -> void;
};

constexpr from_enum_t from_enum{};

} // namespace reflected_scope
} // namespace ryml
} // namespace omni

#include <omnirefl/serialization/ryml/impl/enum.hpp>
