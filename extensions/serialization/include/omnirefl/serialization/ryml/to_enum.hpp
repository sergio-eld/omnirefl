#pragma once

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/reflected_scope.hpp>
#include <omnirefl/serialization/ryml/detail.hpp>

#include <ryml.hpp>

#include <functional>
#include <type_traits>

namespace omni {
namespace ryml {
namespace reflected_scope {

/** Accept declared enumerator names and configured string aliases.
 * Declared names take precedence over aliases. The resolver maps an input
 * alias to a declared name; an empty result means no alias. Diagnostics
 * retain the original document value. with_alias_by_name replaces the
 * resolver, whose returned views must remain valid throughout mapping.
 */
struct to_enum_t {
  template <typename T>
  static constexpr bool supports() {
    return omni::is_reflected_enum<T>::value;
  }

  // TODO: [high] Define a public mapping context interface/type for
  // customization points instead of exposing detail::mapping_context.
  template <typename T>
  auto operator()(::ryml::ConstNodeRef node,
    T *into,
    const detail::mapping_context &context) const
    -> compat::enable_if_t<omni::is_reflected_enum<T>::value,
      with_diagnostics<compat::monostate, diagnostics<false>>>;

  // An empty resolver means no aliases. Returned views must remain valid
  // while the caller uses the resolved name.
  std::function<compat::optional<compat::string_view>(compat::string_view)>
    alias_by_name;

  template <typename F>
  to_enum_t with_alias_by_name(F f) const {
    static_assert(std::is_copy_constructible<F>::value,
      "with_alias_by_name requires a copy-constructible callable");

    static_assert(std::is_move_constructible<F>::value,
      "with_alias_by_name requires a move-constructible callable");

    static_assert(detail::is_alias_by_name<F>::value,
      "Invalid alias_by_name callable; expected: "
      "alias_by_name :: compat::string_view -> optional<compat::string_view>");

    return {/*alias_by_name=*/f};
  }
};

/** Default enum mapper; with_alias_by_name returns a configured copy. */
const to_enum_t to_enum{};

} // namespace reflected_scope
} // namespace ryml
} // namespace omni
