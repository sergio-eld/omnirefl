#pragma once

/** Array and object mappings for element and key/value ranges. */

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/traits.hpp>
#include <omnirefl/serialization/ryml/compose.hpp>
#include <omnirefl/serialization/ryml/impl/shared.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

namespace omni {
namespace ryml {

/** Maps arrays into element ranges and objects into key/value ranges.
 * Does not call \ref reflected_scope::mapping_to_type for the range;
 * element mappings still use it.
 * Elements are mapped before insertion through the shared operation adapters.
 * \ref traits::is_key_value selects element or key/mapped-value arguments;
 * \ref traits::has_unique_key selects try_emplace instead of emplace.
 * Unique destinations apply duplicate policies and require replace for
 * last-wins; non-unique ranges retain every occurrence. Exact repeated
 * document names are selected before value decoding. Destination-specific key equivalence is checked
 * during insertion; earlier conversion errors are retained.
 * Key/value ranges require string-adaptable keys and adapted key/mapped-value
 * types. All destinations require adapted clear and insertion operations.
 * @pre The destination pointer must be non-null and node must belong to a
 * valid tree. The caller validates object/array shape for the destination;
 * the implementation only asserts that precondition.
 */
struct to_range_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<detail::is_mapped_range<T>,
      is_read_state<State>>::value))
  auto operator()(::ryml::ConstNodeRef node, T *into, State context) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;

  // TODO: [high] Define the representation of non-string object keys before
  // enabling their default mapping. JSON object names are always strings.
};

/** Creates a range node-to-type mapping function.
 * Checks for object input for key/value ranges and array input otherwise.
 * Does not call \ref reflected_scope::mapping_to_type for the range;
 * element mappings still use it.
 *
```text
to_range :: type<T> ->
  (node, T*, state) -> with_diagnostics<monostate>
```
 */
template <typename T>
constexpr auto to_range(omni::type_t<T>)
  -> typename traits::select<
    traits::case_<traits::is_key_value<T>::value,
      decltype(compose::with_node_check<core::expect_object_node>(
        to_range_t{}))>,
    decltype(compose::with_node_check<core::expect_array_node>(
      to_range_t{}))>::type {
  return {};
}

/** Maps key/value ranges to objects and element ranges to arrays.
 * Does not call \ref reflected_scope::mapping_from_type for the range;
 * element mappings still use it.
 *
```text
from_range :: (const T&, node, state) -> void
```
 *
 * Empty optional array elements are omitted. Optional object values remain.
 * Non-scalar C++ elements must be exposed as lvalue references to value_type;
 * default mapping rejects temporary-producing iterators in both tree modes. */
struct from_range_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::ryml::detail::is_mapped_range<const T>,
      is_write_state<State>>::value))
  void operator()(const T &value, ::ryml::NodeRef node, State context) const;
};

constexpr from_range_t from_range{};

} // namespace ryml
} // namespace omni

#include <omnirefl/serialization/ryml/impl/sequence.hpp>
