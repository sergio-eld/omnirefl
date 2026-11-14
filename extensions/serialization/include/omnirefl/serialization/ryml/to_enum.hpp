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

/** Deserialize a string-valued node into a reflected enum value.
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
  template <typename T>
  static constexpr bool supports() {
    return omni::is_reflected_enum<T>::value;
  }

  template <typename T,
    compat::enable_if_t<omni::is_reflected_enum<T>::value, int> = 0>
  auto operator()(::ryml::ConstNodeRef node,
    T *into,
    const detail::mapping_context &context) const
    -> with_diagnostics<compat::monostate, diagnostics>;

  ValueByAlias value_by_alias;

  /** QoL for overriding the alias resolver inline, in a single expression.
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
  template <typename F>
  constexpr auto with_value_by_alias(F &&f) const
    -> to_enum_t<compat::decay_t<F>> {
    static_assert(std::is_copy_constructible<compat::decay_t<F>>::value,
      "with_value_by_alias requires a copy-constructible callable");

    static_assert(detail::is_alias_resolver<const compat::decay_t<F>>::value,
      "Invalid value_by_alias callable; expected: "
      "value_by_alias :: compat::string_view -> optional<compat::string_view>");

    return {/*value_by_alias=*/std::forward<F>(f)};
  }
};

/** Default enum deserializer without aliases. */
constexpr to_enum_t<> to_enum{};

} // namespace reflected_scope
} // namespace ryml
} // namespace omni
