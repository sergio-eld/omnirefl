#pragma once

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/traits.hpp>
#include <omnirefl/serialization/ryml/detail.hpp>

#include <ryml.hpp>

namespace omni {
namespace ryml {
namespace mapping {

/** Read arrays into element ranges and objects into key/value ranges.
 * Elements are mapped before insertion through the shared operation adapters.
 * is_key_value selects element versus key/mapped-value arguments;
 * is_unique_key selects try_emplace instead of emplace. Unique destinations
 * apply duplicate policies and require replace for last-wins; non-unique
 * ranges retain every occurrence. Exact repeated document names are selected
 * before value decoding. Destination-specific key equivalence is checked
 * during insertion; earlier conversion errors are retained.
 * Key/value ranges currently require string-adaptable key_type and expose
 * mapped_type. All destinations require adapted clear and insertion operations.
 */
struct to_range_t {
  template <typename T>
  static constexpr bool supports() {
    return omni::ryml::detail::is_mapped_range<T>::value;
  }

  // TODO: [high] Define a public mapping context interface/type for
  // customization points instead of exposing detail::mapping_context.
  template <typename T>
  auto operator()(::ryml::ConstNodeRef node,
    T *into,
    const detail::mapping_context &context) const
    -> compat::enable_if_t<detail::is_mapped_range<T>::value,
      with_diagnostics<compat::monostate, diagnostics<false>>>;

  // TODO: [high] Define the representation of non-string object keys before
  // enabling their default mapping. JSON object names are always strings.
};

/** Default range mapper using the shared operation customization points. */
constexpr to_range_t to_range{};

} // namespace mapping
} // namespace ryml
} // namespace omni
