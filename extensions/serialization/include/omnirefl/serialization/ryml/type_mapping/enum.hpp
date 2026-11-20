#pragma once

/** Reflected enum mappings and node-to-type composition. */

#include <omnirefl/extensions/compat.hpp>
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
 * ValueByAlias maps alias text to an optional canonical value text.
 * An empty result means the input is not a registered alias. The default
 * resolver accepts no aliases.
 * Returned canonical-value views must remain valid throughout the mapping call.
 * @pre Caller validates string input in a valid tree; a debug assertion
 * checks this precondition. Metadata for T must be available in reflected
 * scope.
 */
template <typename ValueByAlias = fn::default_value<
  compat::optional<compat::string_view>>>
struct map_node_to_enum_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::is_reflected_enum<T>,
      is_read_state<State>>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, omni::type_t<T>) const
    -> with_diagnostics_t<T, diagnostics>;

  ValueByAlias value_by_alias;

#if 0
  // TODO: [high] Restore this functional update on the model interface;
  // alias configuration does not belong to the mapping implementation.
  /** (QoL) Transforms data member `.value_by_alias` inplace in an expression.
   * Returns a new mapping function with f(current resolver).
   * The returned resolver must be copy-constructible and const-invocable as
   * string_view -> optional<string_view>, with stable results for each input.
   * Returned views must remain valid throughout the mapping call.
   */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::conjunction<
      std::is_copy_constructible<compat::decay_t<
        compat::invoke_result_t<F &&, const ValueByAlias &>>>,
      omni::ryml::detail::is_alias_resolver<const compat::decay_t<
        compat::invoke_result_t<F &&, const ValueByAlias &>>>>::value))
  constexpr auto tdm_value_by_alias(F &&f) const
    -> map_node_to_enum_t<compat::decay_t<
      compat::invoke_result_t<F &&, const ValueByAlias &>>> {
    return {/*value_by_alias=*/compat::invoke(
      std::forward<F>(f), this->value_by_alias)};
  }
#endif

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
  // Ad hoc: pre-C++17 shared identity; constexpr definition needs a
  // complete self-type and therefore belongs in the implementation header.
  static const map_node_to_enum_t _default;
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_node_to_enum_t<> map_node_to_enum{};
#else
static constexpr const map_node_to_enum_t<> &map_node_to_enum =
  map_node_to_enum_t<>::_default;
#endif

} // namespace reflected_scope
} // namespace core

namespace reflected_scope {

/** Creates an enum node-to-type mapping function.
 * Checks for string input before looking up enumerators.
 * Does not call \ref omni::ryml::reflected_scope::mapping_to_type.
 *
```
default_to_enum :: type<T> -> {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
```
 */
template <typename T>
constexpr auto default_to_enum(omni::type_t<T>)
  -> decltype(compose::with_mutating_node_to_type_overload(
    compose::detail::with_node_check_t<core::expect_string_node,
      core::reflected_scope::map_node_to_enum_t<>>{})) {
  return {};
}

/** Maps a reflected enum value to its declared name as a quoted scalar.
 * Does not call \ref omni::ryml::reflected_scope::mapping_from_type.
 *
```
from_enum :: (state, T, node) -> void
```
 *
 * @pre Metadata for T must be available in reflected scope. */
template <typename = void>
struct from_enum_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<omni::is_reflected_enum<T>,
      is_write_state<State>>::value))
  auto operator()(State, T value, ::ryml::NodeRef node) const -> void;

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
  // Ad hoc: pre-C++17 shared identity; constexpr definition needs a
  // complete self-type and therefore belongs in the implementation header.
  static const from_enum_t _default;
#endif
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr from_enum_t<> from_enum{};
#else
static constexpr const from_enum_t<> &from_enum =
  from_enum_t<>::_default;
#endif

} // namespace reflected_scope
} // namespace ryml
} // namespace omni

#include <omnirefl/serialization/ryml/impl/enum.hpp>
