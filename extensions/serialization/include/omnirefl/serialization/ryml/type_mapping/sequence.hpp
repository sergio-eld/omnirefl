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

/** Maps array nodes into element ranges and object nodes into key/value ranges.
 *
```
to_range :: (state, node, T*) -> with_diagnostics<monostate>
```
 *
 * Elements are mapped through state.map_node.
 * @pre into is non-null; node is valid and has the destination's shape.
 * Object keys are string-like; range operations use \ref adapt::clear and
 * the insertion adapters. */
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
 *
```
map_range :: (state, const T&, node) -> void
```
 *
 * Elements are mapped through state.map_node; empty optional array elements
 * are omitted. \ref traits::is_key_value selects object or array output.
 * @pre Non-scalar elements are exposed as lvalue references to value_type. */
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
