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
 * tdm_value_by_alias transforms the resolver into a new mapper.
 * Returned canonical-value views must remain valid throughout the mapping call.
 */
template <typename ValueByAlias = fn::default_value<
  compat::optional<compat::string_view>>>
struct to_enum_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::is_reflected_enum<T>,
      is_read_state<State>>::value))
  auto operator()(::ryml::ConstNodeRef node, omni::type_t<T>, State context) const
    -> with_diagnostics_t<T, diagnostics>;

  /** (QoL) Mutating adapter for the value-producing enum mapping.
   * Assigns a successfully read enum and returns monostate with its diagnostics.
   * @pre into must be non-null. */
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::is_reflected_enum<T>,
      is_read_state<State>>::value))
  auto operator()(::ryml::ConstNodeRef node, T *into, State context) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;

  ValueByAlias value_by_alias;

  /** (QoL) Transform data member `.value_by_alias` inplace in an expression.
   * Return a new mapper with f(current resolver); the source is unchanged.
   * The returned resolver must be copy-constructible and const-invocable as
   * string_view -> optional<string_view>, with stable results for each input.
   * Returned views must remain valid throughout the mapping call.
   */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::conjunction<
      std::is_copy_constructible<compat::decay_t<
        compat::invoke_result_t<F &&, const ValueByAlias &>>>,
      omni::ryml::detail::is_alias_resolver<const compat::decay_t<
        compat::invoke_result_t<F &&, const ValueByAlias &>>>>::value))
  constexpr auto tdm_value_by_alias(F &&f) const
    -> to_enum_t<compat::decay_t<
      compat::invoke_result_t<F &&, const ValueByAlias &>>> {
    return {/*value_by_alias=*/compat::invoke(
      std::forward<F>(f), this->value_by_alias)};
  }
};

constexpr to_enum_t<> to_enum{};

/** Write a declared enumerator name as a quoted scalar without recursive
 * mapping_from_type dispatch. Direct calls bypass mapping_from_type selection. */
struct from_enum_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::is_reflected_enum<T>,
      is_write_state<State>>::value))
  auto operator()(T value, ::ryml::NodeRef node, State) const -> void;
};

constexpr from_enum_t from_enum{};

} // namespace reflected_scope
} // namespace ryml
} // namespace omni

#include <omnirefl/serialization/ryml/impl/enum.hpp>
