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

/** Read accessible non-static data members from a mapping.
 * Traversal follows the record schema over the document and implements the
 * JSON mapping policies in config.
 * Canonical presence suppresses aliases even when the canonical value is
 * invalid. Duplicate policies select among canonical occurrences or, only
 * when absent, among aliases. Diagnostics retain the original document keys.
 * with_alias_by_name replaces the resolver;
 * it accepts string_view -> optional<string_view>, where an empty result means
 * no alias. Resolved names must remain valid throughout this mapping call.
 */
struct to_record_t {
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
    -> compat::enable_if_t<omni::is_reflected_record<T>::value,
      with_diagnostics<compat::monostate, diagnostics<false>>>;

  // An empty resolver means no aliases. Returned views must remain valid
  // while the caller uses the resolved name.
  std::function<compat::optional<compat::string_view>(compat::string_view)>
    alias_by_name;

  template <typename F>
  to_record_t with_alias_by_name(F f) const {
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

/** Default record mapper; with_alias_by_name returns a configured copy. */
const to_record_t to_record{};

} // namespace reflected_scope
} // namespace ryml
} // namespace omni
