#pragma once

/** Reflected enum mappings and node-to-type composition. */

#include <omnirefl/serialization/ryml/impl/tdm_enum.hpp>
#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/mapping_strategy.hpp>
#include <omnirefl/functional.hpp>
#include <omnirefl/reflected_scope.hpp>
#include <omnirefl/serialization/ryml/compose.hpp>
#include <omnirefl/serialization/ryml/impl/shared.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {
namespace core {
namespace reflected_scope {

/** Maps a string-valued node to a reflected enum value.
 * Resolves generated enumerator names and configured aliases without calling
 * \ref omni::ryml::reflected_scope::mapping_to_type.
 * Declared enumerator names take precedence over aliases. Unknown names
 * produce diagnostics identifying the original input node.
 *
 * @pre Caller validates string input in a valid tree; a debug assertion
 * checks this precondition. Metadata for T must be available in reflected
 * scope.
 */
template <typename Strategy = mapping::enum_strategy_t<>>
struct map_node_to_enum_t:
    detail::tdm_for_map_node_to_enum<Strategy> {
  using tdm_provider = detail::tdm_for_map_node_to_enum<Strategy>;

  static_assert(mapping::is_enum_strategy<Strategy>::value,
    "map_node_to_enum_t requires mapping::enum_strategy_t");

  /** \ref mapping::enum_strategy_t */
  Strategy strategy OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_strategy;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::is_reflected_enum<T>,
      is_read_state<State>>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, omni::type_t<T>) const
    -> with_diagnostics_t<T, diagnostics>;

// Ad hoc: classes with bases are not aggregates before C++17. These
// constructors preserve field initialization in the older standards.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr map_node_to_enum_t() = default;

  OMNI_CONSTRAINED_TEMPLATE((typename Value = Strategy),
    (std::is_constructible<Strategy, Value &&>::value))
  constexpr map_node_to_enum_t(Value &&value)
    : strategy(std::forward<Value>(value)) {}
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_node_to_enum_t<> map_node_to_enum{};
#else
static constexpr const map_node_to_enum_t<> &map_node_to_enum =
  compat::detail::shared_state<map_node_to_enum_t<>>::value;
#endif

} // namespace reflected_scope
} // namespace core

namespace reflected_scope {

/** Creates an enum node-to-type mapping function.
 * Checks for string input before looking up enumerators.
 * Does not call \ref omni::ryml::reflected_scope::mapping_to_type.
 *
```
default_to_enum :: (type<T>, strategy = enum_strategy) -> {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
```
 */
template <typename T, typename Strategy = mapping::enum_strategy_t<>>
constexpr auto default_to_enum(omni::type_t<T>, Strategy strategy = {})
  -> compose::detail::with_mutating_node_to_type_overload_t<
    compose::detail::with_node_check_t<core::expect_string_node,
      core::reflected_scope::map_node_to_enum_t<Strategy>>> {
  return compose::with_mutating_node_to_type_overload(
    compose::with_node_check<core::expect_string_node>(
      core::reflected_scope::map_node_to_enum_t<Strategy>{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
        /*base=*/{},
#endif
        std::move(strategy)}));
}

/** Maps a reflected enum value to its declared name as a quoted scalar.
 * Does not call \ref omni::ryml::reflected_scope::mapping_from_type.
 *
```
map_enum :: (state, T, node) -> void
```
 *
 * @pre Metadata for T must be available in reflected scope. */
template <typename Strategy = mapping::enum_strategy_t<>>
struct map_enum_t:
    detail::tdm_for_map_enum<Strategy> {
  using tdm_provider = detail::tdm_for_map_enum<Strategy>;

  static_assert(mapping::is_enum_strategy<Strategy>::value,
    "map_enum_t requires mapping::enum_strategy_t");

  /** \ref mapping::enum_strategy_t */
  Strategy strategy OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_strategy;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::is_reflected_enum<T>,
      is_write_state<State>>::value))
  auto operator()(State, T value, ::ryml::NodeRef node) const -> void;

// Ad hoc: classes with bases are not aggregates before C++17. These
// constructors preserve field initialization in the older standards.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr map_enum_t() = default;

  OMNI_CONSTRAINED_TEMPLATE((typename Value = Strategy),
    (std::is_constructible<Strategy, Value &&>::value))
  constexpr map_enum_t(Value &&value)
    : strategy(std::forward<Value>(value)) {}
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_enum_t<> map_enum{};
#else
static constexpr const map_enum_t<> &map_enum =
  compat::detail::shared_state<map_enum_t<>>::value;
#endif

} // namespace reflected_scope
} // namespace ryml
} // namespace omni

#include <omnirefl/serialization/ryml/impl/enum.hpp>
