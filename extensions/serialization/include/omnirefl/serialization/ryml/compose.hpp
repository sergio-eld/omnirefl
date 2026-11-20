#pragma once

/** Composition of node-to-type mappings without generated metadata.
 * Callbacks may use `omni::type_t<T>` to select the destination. */

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/functional.hpp>
#include <omnirefl/serialization/ryml/core.hpp>
#include <omnirefl/serialization/ryml/state.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <cassert>
#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {
namespace compose {

/** A node check returning the input node, or an issue on mismatch.
 * For example, core::expect_object_node. */
using node_check = compat::expected<::ryml::ConstNodeRef, issue> (*)(
  ::ryml::ConstNodeRef);

namespace detail {

template <node_check Check, typename Mapping>
struct with_node_check_t;

template <typename Mapping>
struct with_mutating_node_to_type_overload_t;

template <typename Function>
struct lifted_node_function;

} // namespace detail

/** C++11-compatible higher-order function composing a node check and mapping.
 *
```
Check :: node -> expected<node, issue>
state :: read_state_like

Mapping :: (state, node, type<T>) -> with_diagnostics<T>
Mapping :: (state, node, T*) -> with_diagnostics<monostate>

with_node_check<Check> :: Mapping -> Mapping
```
 *
 * Mapping may provide either or both destination overloads. A failed check
 * returns its issue without calling Mapping. Issues with a source node ID
 * use document paths. Issues without an ID use paths relative to the input
 * node and are attributed to that node. An exceeded diagnostic limit also
 * skips Mapping. The owned state is moved into Mapping after validation.
 *
 * An aggregate wrapper preserves constexpr factory construction and default
 * construction for stateless callbacks in every supported standard.
 * @pre The node belongs to a valid tree. Mapping's preconditions apply. */
template <node_check Check, typename Mapping>
constexpr auto with_node_check(Mapping &&mapping)
  OMNI_CPP11_TRAILING_RETURN(
    detail::with_node_check_t<Check, compat::decay_t<Mapping>>) {
  return detail::with_node_check_t<Check, compat::decay_t<Mapping>>{
    /*lifted_mapping=*/std::forward<Mapping>(mapping)};
}

/** (QoL) Adds assignment support to a value-producing node mapping.
 *
```
Mapping :: (state, node, type<T>) -> with_diagnostics<T, Diagnostics>
Mapping :: (state, node, T*) -> with_diagnostics<monostate, Diagnostics>

with_mutating_node_to_type_overload :: Mapping -> {
  (state, node, type<T>) -> with_diagnostics<T, Diagnostics>,
  (state, node, T*)      -> with_diagnostics<monostate, Diagnostics>
}
```
 *
 * Mapping may provide either overload or both. The adapter forwards any
 * existing overload and moves state into it. A T* call uses the existing
 * mutating overload when available. Otherwise it assigns the available result
 * of a value-producing call and retains its diagnostics. This assignment
 * requires T & to be assignable from T &&. A pointer-only mapping exposes
 * only the T* overload.
 *
 * @pre The destination pointer must be non-null. Wrapped mapping
 * preconditions apply. An empty result gives no guarantee about the
 * destination's state.
 */
template <typename Mapping>
constexpr auto with_mutating_node_to_type_overload(Mapping &&mapping)
  -> detail::with_mutating_node_to_type_overload_t<compat::decay_t<Mapping>> {
  return {/*mapping=*/std::forward<Mapping>(mapping)};
}

/** (QoL) Higher-order function lifting node callbacks to with_diagnostics
 * mappings.
 * Expected errors become diagnostics; existing with_diagnostics results pass
 * through. Optional type-tag and state arguments are supplied when accepted.
 *
```
Error :: issue | input_range<issue>
Result<T> :: expected<T, Error> | with_diagnostics<T>
state :: read_state_like

ValueFunction :: node -> Result<T>
ValueFunction :: (state, node) -> Result<T>
ValueFunction :: (state, node, type<T>) -> Result<T>
MutatingFunction :: (node, T*) -> Result<monostate>
MutatingFunction :: (state, node, T*) -> Result<monostate>

lift_to_with_diagnostics(ValueFunction) ::
  (state, node, type<T>) -> with_diagnostics<T>
lift_to_with_diagnostics(MutatingFunction) ::
  (state, node, T*) -> with_diagnostics<monostate>
```
 *
 * Type-tag callbacks receive state. For generic (state, node) callbacks,
 * declare the node parameter as ::ryml::ConstNodeRef or constrain the state
 * parameter to distinguish the callback from the mutating (node, T*) form.
 *
 * Destination overloads come from the callback. No value is default-constructed
 * or assigned by this adapter. A pointer-only callback provides no type<T>
 * overload; a value-producing callback provides no generated T* overload.
 *
 * Errors with a source node ID retain their complete document paths. Errors
 * without a source ID use paths relative to the input node. Issue order is
 * retained. with_diagnostics results pass through with complete diagnostics.
 * Callbacks are invoked as const objects and receive an owned state snapshot.
 * The node is passed unchanged; this
 * adapter does not check its shape or select a registered type mapping.
 * @pre The node belongs to a valid tree; destination pointers are non-null. */
template <typename Function>
constexpr auto lift_to_with_diagnostics(Function &&f)
  -> detail::lifted_node_function<compat::decay_t<Function>> {
  return {/*function=*/std::forward<Function>(f)};
}

/** (QoL) Higher-order function for mapping JSON/YAML objects through f.
 * Object input reaches f; other node shapes return an expected-object
 * diagnostic without calling f.
 *
```
Error :: issue | input_range<issue>
Result<T> :: expected<T, Error> | with_diagnostics<T>
state :: read_state_like

ValueFunction :: node -> Result<T>
ValueFunction :: (state, node) -> Result<T>
ValueFunction :: (state, node, type<T>) -> Result<T>
MutatingFunction :: (node, T*) -> Result<monostate>
MutatingFunction :: (state, node, T*) -> Result<monostate>

to_record(ValueFunction) :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
to_record(MutatingFunction) ::
  (state, node, T*) -> with_diagnostics<monostate>
```
 *
 * A value-producing callback's T* adapter requires assignment from its result.
 * A pointer-only callback provides no type<T> overload. Callbacks may return
 * with_diagnostics instead of expected. Errors with a source node ID keep
 * document paths; otherwise paths are relative to the input node.
 * with_diagnostics carries complete paths and configuration.
 *
 * Callbacks are invoked through a const object. For generic (state, node)
 * callbacks, declare the node parameter as ::ryml::ConstNodeRef or constrain
 * the state parameter to distinguish the callback from (node, T*).
 *
 * Field conversion and alias handling belong to f.
 * The compositor does not call reflected_scope::mapping_to_type.
 * @pre The node belongs to a valid tree; destination pointers are non-null. */
template <typename Function>
constexpr auto to_record(Function &&f)
  OMNI_CPP11_TRAILING_RETURN(decltype(with_mutating_node_to_type_overload(
    with_node_check<core::expect_object_node>(
      lift_to_with_diagnostics(std::forward<Function>(f)))))) {
  return with_mutating_node_to_type_overload(
    with_node_check<core::expect_object_node>(
      lift_to_with_diagnostics(std::forward<Function>(f))));
}

/** (QoL) Higher-order function for mapping JSON/YAML arrays through f.
 * Array input reaches f; other node shapes return an expected-array
 * diagnostic without calling f.
 *
```
Error :: issue | input_range<issue>
Result<T> :: expected<T, Error> | with_diagnostics<T>
state :: read_state_like

ValueFunction :: node -> Result<T>
ValueFunction :: (state, node) -> Result<T>
ValueFunction :: (state, node, type<T>) -> Result<T>
MutatingFunction :: (node, T*) -> Result<monostate>
MutatingFunction :: (state, node, T*) -> Result<monostate>

to_range(ValueFunction) :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
to_range(MutatingFunction) ::
  (state, node, T*) -> with_diagnostics<monostate>
```
 *
 * A value-producing callback's T* adapter requires assignment from its result.
 * A pointer-only callback provides no type<T> overload. Callbacks may return
 * with_diagnostics instead of expected. Errors with a source node ID keep
 * document paths; otherwise paths are relative to the input node.
 * with_diagnostics carries complete paths and configuration.
 *
 * Callbacks are invoked through a const object. For generic (state, node)
 * callbacks, declare the node parameter as ::ryml::ConstNodeRef or constrain
 * the state parameter to distinguish the callback from (node, T*).
 *
 * Element conversion belongs to f. Object-shaped ranges use to_record.
 * The compositor does not call reflected_scope::mapping_to_type.
 * @pre The node belongs to a valid tree; destination pointers are non-null. */
template <typename Function>
constexpr auto to_range(Function &&f)
  OMNI_CPP11_TRAILING_RETURN(decltype(with_mutating_node_to_type_overload(
    with_node_check<core::expect_array_node>(
      lift_to_with_diagnostics(std::forward<Function>(f)))))) {
  return with_mutating_node_to_type_overload(
    with_node_check<core::expect_array_node>(
      lift_to_with_diagnostics(std::forward<Function>(f))));
}

/** (QoL) Higher-order function for enum mappings from JSON/YAML strings.
 * String input reaches f; other node shapes return an expected-string
 * diagnostic without calling f. Quotes and explicit YAML string tags are
 * honored.
 *
```
Error :: issue | input_range<issue>
Result<T> :: expected<T, Error> | with_diagnostics<T>
state :: read_state_like

ValueFunction :: node -> Result<T>
ValueFunction :: (state, node) -> Result<T>
ValueFunction :: (state, node, type<T>) -> Result<T>
MutatingFunction :: (node, T*) -> Result<monostate>
MutatingFunction :: (state, node, T*) -> Result<monostate>

to_enum(ValueFunction) :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
to_enum(MutatingFunction) ::
  (state, node, T*) -> with_diagnostics<monostate>
```
 *
 * A value-producing callback's T* adapter requires assignment from its result.
 * A pointer-only callback provides no type<T> overload. Callbacks may return
 * with_diagnostics instead of expected. Errors with a source node ID keep
 * document paths; otherwise paths are relative to the input node.
 * with_diagnostics carries complete paths and configuration.
 *
 * Callbacks are invoked through a const object. For generic (state, node)
 * callbacks, declare the node parameter as ::ryml::ConstNodeRef or constrain
 * the state parameter to distinguish the callback from (node, T*).
 *
 * Enumerator lookup and aliases belong to f; no enum metadata is queried.
 * The compositor does not call reflected_scope::mapping_to_type.
 * @pre The node belongs to a valid tree; destination pointers are non-null. */
template <typename Function>
constexpr auto to_enum(Function &&f)
  OMNI_CPP11_TRAILING_RETURN(decltype(with_mutating_node_to_type_overload(
    with_node_check<core::expect_string_node>(
      lift_to_with_diagnostics(std::forward<Function>(f)))))) {
  return with_mutating_node_to_type_overload(
    with_node_check<core::expect_string_node>(
      lift_to_with_diagnostics(std::forward<Function>(f))));
}

/** (QoL) Higher-order function for string-like mappings from JSON/YAML strings.
 * String input reaches f; other node shapes return an expected-string
 * diagnostic without calling f. Quotes and explicit YAML string tags are
 * honored.
 *
```
Error :: issue | input_range<issue>
Result<T> :: expected<T, Error> | with_diagnostics<T>
state :: read_state_like

ValueFunction :: node -> Result<T>
ValueFunction :: (state, node) -> Result<T>
ValueFunction :: (state, node, type<T>) -> Result<T>
MutatingFunction :: (node, T*) -> Result<monostate>
MutatingFunction :: (state, node, T*) -> Result<monostate>

to_string_like(ValueFunction) :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
to_string_like(MutatingFunction) ::
  (state, node, T*) -> with_diagnostics<monostate>
```
 *
 * A value-producing callback's T* adapter requires assignment from its result.
 * A pointer-only callback provides no type<T> overload. Callbacks may return
 * with_diagnostics instead of expected. Errors with a source node ID keep
 * document paths; otherwise paths are relative to the input node.
 * with_diagnostics carries complete paths and configuration.
 *
 * Callbacks are invoked through a const object. For generic (state, node)
 * callbacks, declare the node parameter as ::ryml::ConstNodeRef or constrain
 * the state parameter to distinguish the callback from (node, T*).
 *
 * Text conversion and ownership belong to f.
 * The compositor does not call reflected_scope::mapping_to_type.
 * @pre The node belongs to a valid tree; destination pointers are non-null. */
template <typename Function>
constexpr auto to_string_like(Function &&f)
  OMNI_CPP11_TRAILING_RETURN(decltype(with_mutating_node_to_type_overload(
    with_node_check<core::expect_string_node>(
      lift_to_with_diagnostics(std::forward<Function>(f)))))) {
  return with_mutating_node_to_type_overload(
    with_node_check<core::expect_string_node>(
      lift_to_with_diagnostics(std::forward<Function>(f))));
}

/** (QoL) Higher-order function for mappings from JSON/YAML Boolean nodes.
 * Unquoted true/false text reaches f; other input returns an
 * expected-Boolean diagnostic without calling f. Lowercase, title case, and
 * uppercase spellings and explicit YAML Boolean tags are accepted.
 *
```
Error :: issue | input_range<issue>
Result<T> :: expected<T, Error> | with_diagnostics<T>
state :: read_state_like

ValueFunction :: node -> Result<T>
ValueFunction :: (state, node) -> Result<T>
ValueFunction :: (state, node, type<T>) -> Result<T>
MutatingFunction :: (node, T*) -> Result<monostate>
MutatingFunction :: (state, node, T*) -> Result<monostate>

to_boolean(ValueFunction) :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
to_boolean(MutatingFunction) ::
  (state, node, T*) -> with_diagnostics<monostate>
```
 *
 * A value-producing callback's T* adapter requires assignment from its result.
 * A pointer-only callback provides no type<T> overload. Callbacks may return
 * with_diagnostics instead of expected. Errors with a source node ID keep
 * document paths; otherwise paths are relative to the input node.
 * with_diagnostics carries complete paths and configuration.
 *
 * Callbacks are invoked through a const object. For generic (state, node)
 * callbacks, declare the node parameter as ::ryml::ConstNodeRef or constrain
 * the state parameter to distinguish the callback from (node, T*).
 *
 * Conversion to the destination value belongs to f.
 * The compositor does not call reflected_scope::mapping_to_type.
 * @pre The node belongs to a valid tree; destination pointers are non-null. */
template <typename Function>
constexpr auto to_boolean(Function &&f)
  OMNI_CPP11_TRAILING_RETURN(decltype(with_mutating_node_to_type_overload(
    with_node_check<core::expect_boolean_node>(
      lift_to_with_diagnostics(std::forward<Function>(f)))))) {
  return with_mutating_node_to_type_overload(
    with_node_check<core::expect_boolean_node>(
      lift_to_with_diagnostics(std::forward<Function>(f))));
}

/** (QoL) Higher-order function for mappings from JSON/YAML integer nodes.
 * Unquoted integer text reaches f; quoted numbers, real numbers, and
 * other input return an expected-integer diagnostic without calling f.
 *
```
Error :: issue | input_range<issue>
Result<T> :: expected<T, Error> | with_diagnostics<T>
state :: read_state_like

ValueFunction :: node -> Result<T>
ValueFunction :: (state, node) -> Result<T>
ValueFunction :: (state, node, type<T>) -> Result<T>
MutatingFunction :: (node, T*) -> Result<monostate>
MutatingFunction :: (state, node, T*) -> Result<monostate>

to_integral(ValueFunction) :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
to_integral(MutatingFunction) ::
  (state, node, T*) -> with_diagnostics<monostate>
```
 *
 * A value-producing callback's T* adapter requires assignment from its result.
 * A pointer-only callback provides no type<T> overload. Callbacks may return
 * with_diagnostics instead of expected. Errors with a source node ID keep
 * document paths; otherwise paths are relative to the input node.
 * with_diagnostics carries complete paths and configuration.
 *
 * Callbacks are invoked through a const object. For generic (state, node)
 * callbacks, declare the node parameter as ::ryml::ConstNodeRef or constrain
 * the state parameter to distinguish the callback from (node, T*).
 *
 * Conversion and destination range checks belong to f.
 * The compositor does not call reflected_scope::mapping_to_type.
 * @pre The node belongs to a valid tree; destination pointers are non-null. */
template <typename Function>
constexpr auto to_integral(Function &&f)
  OMNI_CPP11_TRAILING_RETURN(decltype(with_mutating_node_to_type_overload(
    with_node_check<core::expect_integer_node>(
      lift_to_with_diagnostics(std::forward<Function>(f)))))) {
  return with_mutating_node_to_type_overload(
    with_node_check<core::expect_integer_node>(
      lift_to_with_diagnostics(std::forward<Function>(f))));
}

/** (QoL) Higher-order function for mappings from JSON/YAML numeric nodes.
 * Unquoted integer or real text reaches f; other input returns an
 * expected-number diagnostic without calling f.
 *
```
Error :: issue | input_range<issue>
Result<T> :: expected<T, Error> | with_diagnostics<T>
state :: read_state_like

ValueFunction :: node -> Result<T>
ValueFunction :: (state, node) -> Result<T>
ValueFunction :: (state, node, type<T>) -> Result<T>
MutatingFunction :: (node, T*) -> Result<monostate>
MutatingFunction :: (state, node, T*) -> Result<monostate>

to_floating_point(ValueFunction) :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
to_floating_point(MutatingFunction) ::
  (state, node, T*) -> with_diagnostics<monostate>
```
 *
 * A value-producing callback's T* adapter requires assignment from its result.
 * A pointer-only callback provides no type<T> overload. Callbacks may return
 * with_diagnostics instead of expected. Errors with a source node ID keep
 * document paths; otherwise paths are relative to the input node.
 * with_diagnostics carries complete paths and configuration.
 *
 * Callbacks are invoked through a const object. For generic (state, node)
 * callbacks, declare the node parameter as ::ryml::ConstNodeRef or constrain
 * the state parameter to distinguish the callback from (node, T*).
 *
 * Conversion and destination range checks belong to f.
 * The compositor does not call reflected_scope::mapping_to_type.
 * @pre The node belongs to a valid tree; destination pointers are non-null. */
template <typename Function>
constexpr auto to_floating_point(Function &&f)
  OMNI_CPP11_TRAILING_RETURN(decltype(with_mutating_node_to_type_overload(
    with_node_check<core::expect_number_node>(
      lift_to_with_diagnostics(std::forward<Function>(f)))))) {
  return with_mutating_node_to_type_overload(
    with_node_check<core::expect_number_node>(
      lift_to_with_diagnostics(std::forward<Function>(f))));
}

/** (QoL) Higher-order function for mappings from Boolean or numeric nodes.
 * JSON/YAML Boolean, integer, or real input reaches f; other input
 * returns an expected-scalar diagnostic without calling f.
 *
```
Error :: issue | input_range<issue>
Result<T> :: expected<T, Error> | with_diagnostics<T>
state :: read_state_like

ValueFunction :: node -> Result<T>
ValueFunction :: (state, node) -> Result<T>
ValueFunction :: (state, node, type<T>) -> Result<T>
MutatingFunction :: (node, T*) -> Result<monostate>
MutatingFunction :: (state, node, T*) -> Result<monostate>

to_arithmetic(ValueFunction) :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
to_arithmetic(MutatingFunction) ::
  (state, node, T*) -> with_diagnostics<monostate>
```
 *
 * A value-producing callback's T* adapter requires assignment from its result.
 * A pointer-only callback provides no type<T> overload. Callbacks may return
 * with_diagnostics instead of expected. Errors with a source node ID keep
 * document paths; otherwise paths are relative to the input node.
 * with_diagnostics carries complete paths and configuration.
 *
 * Callbacks are invoked through a const object. For generic (state, node)
 * callbacks, declare the node parameter as ::ryml::ConstNodeRef or constrain
 * the state parameter to distinguish the callback from (node, T*).
 *
 * Selection of the conversion and destination range checks belong to f.
 * The compositor does not call reflected_scope::mapping_to_type.
 * @pre The node belongs to a valid tree; destination pointers are non-null. */
template <typename Function>
constexpr auto to_arithmetic(Function &&f)
  OMNI_CPP11_TRAILING_RETURN(decltype(with_mutating_node_to_type_overload(
    with_node_check<core::expect_arithmetic_node>(
      lift_to_with_diagnostics(std::forward<Function>(f)))))) {
  return with_mutating_node_to_type_overload(
    with_node_check<core::expect_arithmetic_node>(
      lift_to_with_diagnostics(std::forward<Function>(f))));
}

/** (QoL) Higher-order function for mappings from nullable JSON/YAML values.
 * Scalar, object, and array nodes reach f, including explicit null.
 * A node without a value or container returns a diagnostic without calling f.
 *
```
Error :: issue | input_range<issue>
Result<T> :: expected<T, Error> | with_diagnostics<T>
state :: read_state_like

ValueFunction :: node -> Result<T>
ValueFunction :: (state, node) -> Result<T>
ValueFunction :: (state, node, type<T>) -> Result<T>
MutatingFunction :: (node, T*) -> Result<monostate>
MutatingFunction :: (state, node, T*) -> Result<monostate>

to_optional_like(ValueFunction) :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
to_optional_like(MutatingFunction) ::
  (state, node, T*) -> with_diagnostics<monostate>
```
 *
 * A value-producing callback's T* adapter requires assignment from its result.
 * A pointer-only callback provides no type<T> overload. Callbacks may return
 * with_diagnostics instead of expected. Errors with a source node ID keep
 * document paths; otherwise paths are relative to the input node.
 * with_diagnostics carries complete paths and configuration.
 *
 * Callbacks are invoked through a const object. For generic (state, node)
 * callbacks, declare the node parameter as ::ryml::ConstNodeRef or constrain
 * the state parameter to distinguish the callback from (node, T*).
 *
 * Null handling and conversion of present values belong to f.
 * The compositor does not call reflected_scope::mapping_to_type.
 * @pre The node belongs to a valid tree; destination pointers are non-null. */
template <typename Function>
constexpr auto to_optional_like(Function &&f)
  OMNI_CPP11_TRAILING_RETURN(decltype(with_mutating_node_to_type_overload(
    with_node_check<core::expect_value_node>(
      lift_to_with_diagnostics(std::forward<Function>(f)))))) {
  return with_mutating_node_to_type_overload(
    with_node_check<core::expect_value_node>(
      lift_to_with_diagnostics(std::forward<Function>(f))));
}

} // namespace compose

namespace detail {

inline issue node_function_error(issue error, ::ryml::ConstNodeRef node);

} // namespace detail

namespace compose {
namespace detail {

/** Calls a node check before the mapping function, preserving its overloads.
 * A failed check returns its issue with the source node and document path.
 * The mapping function is skipped on failure or an exceeded diagnostic limit.
 *
```
Check :: node -> expected<node, issue>
Mapping :: (state, node, destination) -> with_diagnostics<T>

with_node_check<Check> :: Mapping -> Mapping
```
 *
 * Check is retained in the callable type: default selection constructs the
 * factory's result type, which must preserve its configured check. */
// Ad hoc: built-in selection default-constructs callable types, which captured
// lambdas cannot support. This wrapper also provides the C++11/14
// implementation.
// REFACTORME: [medium] Consider selecting factory-produced default values.
template <node_check Check, typename Mapping>
struct with_node_check_t {
  // Keeping the check in the type preserves it under default construction.
  static constexpr auto expect_node = Check;
  Mapping lifted_mapping;

  // The trailing decltype excludes unsupported wrapped calls through SFINAE.
  OMNI_CONSTRAINED_TEMPLATE((typename Destination, typename State),
    (is_read_state<State>::value))
  auto operator()(State state,
    ::ryml::ConstNodeRef node,
    Destination into) const
    -> decltype(this->lifted_mapping(std::move(state), node, into)) {
    if (state.limit_exceeded())
      return {
        /*value=*/compat::nullopt,
        /*diagnostics=*/{/*config=*/{state.config.error_policy, {}}, {}, {}},
      };

    auto checked = this->expect_node(node);
    if (!checked) {
      return {
        /*value=*/compat::nullopt,
        /*diagnostics=*/{
          /*config=*/{state.config.error_policy, {}},
          /*issues=*/{omni::ryml::detail::node_function_error(
            std::move(checked.error()), node)},
          /*parse_error_message=*/{},
        },
      };
    }

    return this->lifted_mapping(std::move(state), *checked, into);
  }
};

template <typename Mapping>
struct with_mutating_node_to_type_overload_t {
  Mapping mapping;

  // Overload ranking preserves a native pointer call before trying assignment.
  template <typename T, typename State>
  auto map_into(State state, ::ryml::ConstNodeRef node, T *into, int) const
    -> decltype(this->mapping(std::move(state), node, into)) {
    return this->mapping(std::move(state), node, into);
  }

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_assignable<T &, T &&>::value))
  auto map_into(State state, ::ryml::ConstNodeRef node, T *into, long) const
    -> with_diagnostics_t<compat::monostate, compat::decay_t<decltype(
      this->mapping(std::move(state), node, omni::type_t<T>{}).diagnostics)>> {
    return this->mapping(std::move(state), node, omni::type_t<T>{})
      .transform([into](T &&value) -> compat::monostate {
        *into = std::move(value);
        return {};
      });
  }

  // The trailing decltype excludes unsupported wrapped calls through SFINAE.
  template <typename T, typename State>
  auto operator()(State state,
    ::ryml::ConstNodeRef node,
    omni::type_t<T> type) const
    -> decltype(this->mapping(std::move(state), node, type)) {
    return this->mapping(std::move(state), node, type);
  }

  template <typename T, typename State>
  auto operator()(State state, ::ryml::ConstNodeRef node, T *into) const
    -> decltype(this->map_into(std::move(state), node, into, 0)) {
    assert(into);
    return this->map_into(std::move(state), node, into, 0);
  }
};

} // namespace detail
} // namespace compose
} // namespace ryml
} // namespace omni

#include <omnirefl/serialization/ryml/impl/compose.hpp>
