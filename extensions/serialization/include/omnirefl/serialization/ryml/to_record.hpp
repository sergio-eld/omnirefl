#pragma once

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/reflected_scope.hpp>
#include <omnirefl/serialization/ryml/detail.hpp>

#include <ryml.hpp>

#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {
namespace reflected_scope {

/** Read accessible non-static data members from a mapping.
 * Canonical presence suppresses aliases even when the canonical value is
 * invalid. Duplicate policies select among canonical occurrences or, only
 * when absent, among aliases. Diagnostics retain the original document keys.
 * with_alias_by_name replaces the resolver;
 * it accepts string_view -> optional<string_view>, where an empty result means
 * no alias. Resolved names must remain valid throughout this mapping call.
 */
template <typename AliasByName = detail::default_value<
  compat::optional<compat::string_view>>>
struct to_record {
  template <typename T>
  static constexpr bool supports() {
    return omni::is_reflected_record<T>::value;
  }

  // TODO: [high] Define a public mapping context interface/type for
  // customization points instead of exposing detail::mapping_context.
  template <typename T>
  auto operator()(::ryml::ConstNodeRef node,
    T *into,
    const detail::mapping_context &context) const
    -> compat::enable_if_t<omni::is_reflected_record<T>::value, node_result>;

  // Returned views must remain valid while the caller uses the resolved name.
  AliasByName alias_by_name;

  template <typename F>
  OMNI_CPP14_CONSTEXPR to_record<F> with_alias_by_name(F f) const {
    static_assert(std::is_copy_constructible<F>::value,
      "with_alias_by_name requires a copy-constructible callable");
    static_assert(detail::is_alias_by_name<F>::value,
      "Invalid alias_by_name callable; expected: "
      "alias_by_name :: compat::string_view -> optional<compat::string_view>");
    return {/*alias_by_name=*/std::move_if_noexcept(f)};
  }
};

} // namespace reflected_scope
} // namespace ryml
} // namespace omni
