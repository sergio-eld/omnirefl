#pragma once

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/traits.hpp>
#include <omnirefl/serialization/ryml/impl/shared.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

namespace omni {
namespace ryml {

/** Read arrays into element ranges and objects into key/value ranges.
 * Direct calls bypass mapping_to_type selection for the range; element
 * mappings still use mapping_to_type dispatch.
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
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<is_read_state<State>,
      detail::is_mapped_range<T>>::value))
  auto operator()(::ryml::ConstNodeRef node, T *into, State context) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;

  // TODO: [high] Define the representation of non-string object keys before
  // enabling their default mapping. JSON object names are always strings.
};

constexpr to_range_t to_range{};

/** Write key/value ranges as objects and element ranges as arrays.
 * Direct calls bypass mapping_from_type selection for the range; element
 * mappings still use mapping_from_type dispatch.
 * Empty optional array elements are omitted. Optional object values remain.
 * Non-scalar C++ elements must be exposed as lvalue references to value_type;
 * default mapping rejects temporary-producing iterators in both tree modes. */
struct from_range_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<is_write_state<State>,
      omni::ryml::detail::is_mapped_range<const T>>::value))
  void operator()(const T &value, ::ryml::NodeRef node, State context) const;
};

constexpr from_range_t from_range{};

} // namespace ryml
} // namespace omni

#include <omnirefl/serialization/ryml/impl/sequence.hpp>
