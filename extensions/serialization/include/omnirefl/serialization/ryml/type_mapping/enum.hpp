#pragma once

/** Reflected enum mappings and node-to-type composition. */

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/mapping_strategy.hpp>
#include <omnirefl/functional.hpp>
#include <omnirefl/reflected_scope.hpp>
#include <omnirefl/serialization/ryml/compose.hpp>
#include <omnirefl/serialization/ryml/impl/shared.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

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
 * strategy supplies the shared enumerator alias configuration;
 * tdm_strategy returns an adjusted mapping. Serialization keeps canonical
 * enumerator names.
 * @pre Caller validates string input in a valid tree; a debug assertion
 * checks this precondition. Metadata for T must be available in reflected
 * scope.
 */
template <typename Strategy = mapping::enum_strategy_t<>>
struct map_node_to_enum_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::is_reflected_enum<T>,
      is_read_state<State>>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, omni::type_t<T>) const
    -> with_diagnostics_t<T, diagnostics>;

  Strategy strategy OMNI_CPP14_DEFAULT_INIT();

  /** (QoL) Functional update of the shared mapping strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, const Strategy &>::value))
  constexpr auto tdm_strategy(F &&f) const &
    -> map_node_to_enum_t<compat::decay_t<
      compat::invoke_result_t<F &&, const Strategy &>>> {
    return {compat::invoke(std::forward<F>(f), this->strategy)};
  }

  /** (QoL) Functional update consuming the bound strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, Strategy &&>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_strategy(F &&f) &&
    -> map_node_to_enum_t<compat::decay_t<
      compat::invoke_result_t<F &&, Strategy &&>>> {
    return {compat::invoke(std::forward<F>(f),
      std::move(this->strategy))};
  }

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
      core::reflected_scope::map_node_to_enum_t<Strategy>{std::move(strategy)}));
}

/** Maps a reflected enum value to its declared name as a quoted scalar.
 * Does not call \ref omni::ryml::reflected_scope::mapping_from_type.
 *
```
from_enum :: (state, T, node) -> void
```
 *
 * @pre Metadata for T must be available in reflected scope. */
template <typename Strategy = mapping::enum_strategy_t<>>
struct from_enum_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::is_reflected_enum<T>,
      is_write_state<State>>::value))
  auto operator()(State, T value, ::ryml::NodeRef node) const -> void;

  Strategy strategy OMNI_CPP14_DEFAULT_INIT();

  /** (QoL) Functional update of the shared mapping strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, const Strategy &>::value))
  constexpr auto tdm_strategy(F &&f) const &
    -> from_enum_t<compat::decay_t<
      compat::invoke_result_t<F &&, const Strategy &>>> {
    return {compat::invoke(std::forward<F>(f), this->strategy)};
  }

  /** (QoL) Functional update consuming the bound strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, Strategy &&>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_strategy(F &&f) &&
    -> from_enum_t<compat::decay_t<
      compat::invoke_result_t<F &&, Strategy &&>>> {
    return {compat::invoke(std::forward<F>(f),
      std::move(this->strategy))};
  }

};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr from_enum_t<> from_enum{};
#else
static constexpr const from_enum_t<> &from_enum =
  compat::detail::shared_state<from_enum_t<>>::value;
#endif

} // namespace reflected_scope
} // namespace ryml
} // namespace omni

#include <omnirefl/serialization/ryml/impl/enum.hpp>
