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
 * element mappings use state.map_node, which defaults to that lookup.
 * Elements are mapped before insertion through the shared operation adapters.
 * \ref traits::is_key_value selects element or key/mapped-value arguments;
 * \ref traits::has_unique_key selects try_emplace instead of emplace.
 * Unique destinations apply duplicate policies and require replace for
 * last-wins; non-unique ranges retain every occurrence. Exact repeated
 * document names are selected before value decoding. Destination-specific
 * key equivalence is checked during insertion; earlier conversion errors are
 * retained.
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
  auto operator()(State context, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;

  // TODO: [high] Define the representation of non-string object keys before
  // enabling their default mapping. JSON object names are always strings.
};

/** Creates a range node-to-type mapping function.
 * Checks for object input for key/value ranges and array input otherwise.
 * Does not call \ref reflected_scope::mapping_to_type for the range;
 * element mappings use state.map_node, which defaults to that lookup.
 *
```
default_to_range :: type<T> ->
  (state, node, T*) -> with_diagnostics<monostate>
```
 */
template <typename T>
constexpr auto default_to_range(omni::type_t<T>)
  -> typename traits::select<
    traits::case_<traits::is_key_value<T>::value,
      compose::detail::with_node_check_t<core::expect_object_node, to_range_t>>,
    compose::detail::with_node_check_t<core::expect_array_node,
      to_range_t>>::type {
  return {};
}

/** Maps key/value ranges to objects and element ranges to arrays.
 * Does not call \ref reflected_scope::mapping_from_type for the range;
 * element mappings use state.map_node, which defaults to that lookup.
 *
```
from_range :: (state, const T&, node) -> void
```
 *
 * Empty optional array elements are omitted. Optional object values remain.
 * Non-scalar C++ elements must be exposed as lvalue references to value_type;
 * default mapping rejects temporary-producing iterators in both tree modes. */
struct from_range_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::ryml::detail::is_mapped_range<const T>,
      is_write_state<State>>::value))
  void operator()(State context, const T &value, ::ryml::NodeRef node) const;

};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr from_range_t from_range{};
#else
static constexpr const from_range_t &from_range =
  compat::detail::shared_state<from_range_t>::value;
#endif

} // namespace ryml
} // namespace omni

#include <omnirefl/serialization/ryml/impl/sequence.hpp>
