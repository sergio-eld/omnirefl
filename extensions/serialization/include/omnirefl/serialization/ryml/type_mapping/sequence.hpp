#pragma once

/** Array and object mappings for element and key/value ranges. */

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/traits.hpp>
#include <omnirefl/serialization/ryml/core.hpp>
#include <omnirefl/serialization/ryml/impl/shared.hpp>
#include <omnirefl/serialization/ryml/impl/tdm_node.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

namespace omni {
namespace ryml {

/** Maps arrays into element ranges and objects into key/value ranges.
 * Does not call \ref reflected_scope::mapping_to_type for the range;
 * elements are mapped through state.map_node.
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
struct to_range_t: detail::with_depth_exceeded_operator {
  using detail::with_depth_exceeded_operator::operator();

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<detail::is_mapped_range<T>,
      is_read_state<State>>::value))
  auto operator()(State context, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;

  // TODO: [high] Define the representation of non-string object keys before
  // enabling their default mapping. JSON object names are always strings.
};

/** Maps array nodes into element ranges and object nodes into key/value ranges.
 * Elements are mapped through state.map_node.
 *
```
map_range_node :: (state, node, T*) -> with_diagnostics<monostate>
```
 */
template <typename WithNodeCheck = std::true_type>
struct map_range_node_t:
    detail::tdm_for_map_range_node<WithNodeCheck>,
    detail::with_depth_exceeded_operator {
  using tdm_provider = detail::tdm_for_map_range_node<WithNodeCheck>;
  using detail::with_depth_exceeded_operator::operator();

  /** Disable only when the node satisfies the destination's shape. */
  WithNodeCheck with_node_check OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_with_node_check;
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (detail::is_mapped_range<T>::value && is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate>;

// Ad hoc: classes with bases are not aggregates before C++17. These
// constructors preserve field initialization in the older standards.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr map_range_node_t() = default;

  OMNI_CONSTRAINED_TEMPLATE((typename Value = WithNodeCheck),
    (std::is_constructible<WithNodeCheck, Value &&>::value))
  constexpr map_range_node_t(Value &&value)
    : with_node_check(std::forward<Value>(value)) {}
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_range_node_t<> map_range_node{};
#else
static constexpr const map_range_node_t<> &map_range_node =
  compat::detail::shared_state<map_range_node_t<>>::value;
#endif

/** Maps key/value ranges to objects and element ranges to arrays.
 * Does not call \ref reflected_scope::mapping_from_type for the range;
 * elements are mapped through state.map_node.
 *
```
map_range :: (state, const T&, node) -> void
```
 *
 * Empty optional array elements are omitted. Optional object values remain.
 * Non-scalar C++ elements must be exposed as lvalue references to value_type;
 * default mapping rejects temporary-producing iterators in both tree modes. */
struct map_range_t: detail::with_depth_exceeded_operator {
  using detail::with_depth_exceeded_operator::operator();

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::ryml::detail::is_mapped_range<const T>,
      is_write_state<State>>::value))
  void operator()(State context, const T &value, ::ryml::NodeRef node) const;

};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_range_t map_range{};
#else
static constexpr const map_range_t &map_range =
  compat::detail::shared_state<map_range_t>::value;
#endif

} // namespace ryml
} // namespace omni

#include <omnirefl/serialization/ryml/impl/sequence.hpp>
