#pragma once

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/traits.hpp>
#include <omnirefl/serialization/ryml/detail.hpp>

#include <ryml.hpp>

namespace omni {
namespace ryml {

/** Write key/value ranges as objects and element ranges as arrays.
 * Empty optional array elements are omitted. Optional object values remain.
 * Non-scalar C++ elements must be exposed as lvalue references to value_type;
 * default mapping rejects temporary-producing iterators in both tree modes. */
struct from_range_t {
  template <typename T>
  static constexpr bool supports() {
    return omni::ryml::detail::is_mapped_range<const T>::value;
  }

  template <typename T,
    typename Config,
    compat::enable_if_t<omni::ryml::detail::is_mapped_range<const T>::value,
      int> = 0>
  void operator()(const T &value,
    ::ryml::NodeRef node,
    mapping_state<Config> context) const;
};

/** Default range serializer. */
constexpr from_range_t from_range{};

/** Read arrays into element ranges and objects into key/value ranges.
 * Elements are mapped before insertion through the shared operation adapters.
 * is_key_value selects element versus key/mapped-value arguments;
 * has_unique_key selects try_emplace instead of emplace. Unique destinations
 * apply duplicate policies and require replace for last-wins; non-unique
 * ranges retain every occurrence. Exact repeated document names are selected
 * before value decoding. Destination-specific key equivalence is checked
 * during insertion; earlier conversion errors are retained.
 * Key/value ranges require string-adaptable keys and adapted key/mapped-value
 * types. All destinations require adapted clear and insertion operations.
 */
struct to_range_t {
  template <typename T>
  static constexpr bool supports() {
    return omni::ryml::detail::is_mapped_range<T>::value;
  }

  template <typename T, typename Config>
  auto operator()(::ryml::ConstNodeRef node,
    T *into,
    mapping_state<Config> context) const
    -> compat::enable_if_t<detail::is_mapped_range<T>::value,
      with_diagnostics<compat::monostate, diagnostics>>;

  // TODO: [high] Define the representation of non-string object keys before
  // enabling their default mapping. JSON object names are always strings.
};

/** Default range mapper using the shared operation customization points. */
constexpr to_range_t to_range{};

} // namespace ryml
} // namespace omni
