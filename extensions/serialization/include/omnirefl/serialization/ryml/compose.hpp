#pragma once

/** Composition of node-to-type mappings without generated metadata.
 * Callbacks may use `omni::type_t<T>` to select the destination. */

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/functional.hpp>
#include <omnirefl/serialization/ryml/node_checks.hpp>
#include <omnirefl/serialization/ryml/state.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <cassert>
#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {
namespace compose {

/** A node check returning one issue on mismatch, or nullopt on success.
 * For example, core::expect_object_node. */
using node_check = compat::optional<issue> (*)(::ryml::ConstNodeRef);

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
Check :: node -> optional<issue>
state :: read_state_like

Mapping :: (node, type<T>, state) -> with_diagnostics<T>
Mapping :: (node, T*, state) -> with_diagnostics<monostate>

with_node_check<Check> :: Mapping -> Mapping
```
 *
 * Mapping may provide either or both destination overloads. A failed check
 * returns its issue at the current path without calling Mapping. An exceeded
 * diagnostic limit also skips Mapping.
 *
 * Uses a generic lambda from C++17; earlier standards use a wrapper struct
 * to preserve constexpr factory construction.
 * @pre The node belongs to a valid tree; Mapping's other preconditions apply. */
template <node_check Check, typename Mapping>
constexpr auto with_node_check(Mapping &&mapping)
  OMNI_CPP11_TRAILING_RETURN(
    detail::with_node_check_t<Check, compat::decay_t<Mapping>>) {
#if defined(__cpp_constexpr) && 201603L <= __cpp_constexpr
  return //
    [mapping = std::forward<Mapping>(mapping)](::ryml::ConstNodeRef node,
      auto into, auto state)
      -> decltype(std::declval<const compat::decay_t<Mapping> &>()(
        node, into, state)) {
      if (state.limit_exceeded())
        return {
          /*value=*/compat::nullopt,
          /*diagnostics=*/{/*config=*/{state.config.error_policy, {}}, {}, {}},
        };

      if (auto check = Check(node)) {
        auto path = state.path.materialize();
        path.insert(path.end(), check->path.begin(), check->path.end());
        check->path = std::move(path);
        return {
          /*value=*/compat::nullopt,
          /*diagnostics=*/{
            /*config=*/{state.config.error_policy, {}},
            /*issues=*/{std::move(*check)},
            /*parse_error_message=*/{},
          },
        };
      }

      return mapping(node, into, state);
    };
#else
  return detail::with_node_check_t<Check, compat::decay_t<Mapping>>{
    /*lifted_mapping=*/std::forward<Mapping>(mapping)};
#endif
}

/** (QoL) Adds a mutating overload to a value-producing mapping function.
 * An existing mutating overload takes precedence. Otherwise the adapter
 * assigns successfully mapped values and retains the mapping diagnostics.
 * @pre The destination pointer must be non-null. Wrapped mapping
 * preconditions still apply.
 * An empty result gives no guarantee about the destination's state.
 *
```
Mapping :: (node, type<T>, state) -> with_diagnostics<T>

with_mutating_node_to_type_overload :: Mapping -> {
  (node, type<T>, state) -> with_diagnostics<T>,
  (node, T*, state)      -> with_diagnostics<monostate>
}
```
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
ValueFunction :: (node, state) -> Result<T>
ValueFunction :: (node, type<T>) -> Result<T>
ValueFunction :: (node, type<T>, state) -> Result<T>
MutatingFunction :: (node, T*) -> Result<monostate>
MutatingFunction :: (node, T*, state) -> Result<monostate>

lift_to_with_diagnostics(ValueFunction) ::
  (node, type<T>, state) -> with_diagnostics<T>
lift_to_with_diagnostics(MutatingFunction) ::
  (node, T*, state) -> with_diagnostics<monostate>
```
 *
 * Destination overloads come from the callback. No value is default-constructed
 * or assigned by this adapter. A pointer-only callback provides no type<T>
 * overload; a value-producing callback provides no generated T* overload.
 *
 * Expected errors use paths relative to the input node; their order and
 * existing source nodes are kept. with_diagnostics results pass through with
 * complete paths and configuration; callbacks can use state to construct them.
 * Callbacks are invoked as const objects. The node is passed unchanged; this
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
ValueFunction :: (node, state) -> Result<T>
ValueFunction :: (node, type<T>) -> Result<T>
ValueFunction :: (node, type<T>, state) -> Result<T>
MutatingFunction :: (node, T*) -> Result<monostate>
MutatingFunction :: (node, T*, state) -> Result<monostate>

to_record(ValueFunction) :: {
  (node, type<T>, state) -> with_diagnostics<T>,
  (node, T*, state)      -> with_diagnostics<monostate>
}
to_record(MutatingFunction) ::
  (node, T*, state) -> with_diagnostics<monostate>
```
 *
 * A value-producing callback's T* adapter requires assignment from its result.
 * A pointer-only callback provides no type<T> overload. Callbacks may return
 * with_diagnostics instead of expected. Expected errors use node-relative
 * paths; with_diagnostics carries complete paths and configuration.
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
ValueFunction :: (node, state) -> Result<T>
ValueFunction :: (node, type<T>) -> Result<T>
ValueFunction :: (node, type<T>, state) -> Result<T>
MutatingFunction :: (node, T*) -> Result<monostate>
MutatingFunction :: (node, T*, state) -> Result<monostate>

to_range(ValueFunction) :: {
  (node, type<T>, state) -> with_diagnostics<T>,
  (node, T*, state)      -> with_diagnostics<monostate>
}
to_range(MutatingFunction) ::
  (node, T*, state) -> with_diagnostics<monostate>
```
 *
 * A value-producing callback's T* adapter requires assignment from its result.
 * A pointer-only callback provides no type<T> overload. Callbacks may return
 * with_diagnostics instead of expected. Expected errors use node-relative
 * paths; with_diagnostics carries complete paths and configuration.
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
ValueFunction :: (node, state) -> Result<T>
ValueFunction :: (node, type<T>) -> Result<T>
ValueFunction :: (node, type<T>, state) -> Result<T>
MutatingFunction :: (node, T*) -> Result<monostate>
MutatingFunction :: (node, T*, state) -> Result<monostate>

to_enum(ValueFunction) :: {
  (node, type<T>, state) -> with_diagnostics<T>,
  (node, T*, state)      -> with_diagnostics<monostate>
}
to_enum(MutatingFunction) ::
  (node, T*, state) -> with_diagnostics<monostate>
```
 *
 * A value-producing callback's T* adapter requires assignment from its result.
 * A pointer-only callback provides no type<T> overload. Callbacks may return
 * with_diagnostics instead of expected. Expected errors use node-relative
 * paths; with_diagnostics carries complete paths and configuration.
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
ValueFunction :: (node, state) -> Result<T>
ValueFunction :: (node, type<T>) -> Result<T>
ValueFunction :: (node, type<T>, state) -> Result<T>
MutatingFunction :: (node, T*) -> Result<monostate>
MutatingFunction :: (node, T*, state) -> Result<monostate>

to_string_like(ValueFunction) :: {
  (node, type<T>, state) -> with_diagnostics<T>,
  (node, T*, state)      -> with_diagnostics<monostate>
}
to_string_like(MutatingFunction) ::
  (node, T*, state) -> with_diagnostics<monostate>
```
 *
 * A value-producing callback's T* adapter requires assignment from its result.
 * A pointer-only callback provides no type<T> overload. Callbacks may return
 * with_diagnostics instead of expected. Expected errors use node-relative
 * paths; with_diagnostics carries complete paths and configuration.
 * Text conversion, ownership, and destination assignment belong to f.
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
ValueFunction :: (node, state) -> Result<T>
ValueFunction :: (node, type<T>) -> Result<T>
ValueFunction :: (node, type<T>, state) -> Result<T>
MutatingFunction :: (node, T*) -> Result<monostate>
MutatingFunction :: (node, T*, state) -> Result<monostate>

to_boolean(ValueFunction) :: {
  (node, type<T>, state) -> with_diagnostics<T>,
  (node, T*, state)      -> with_diagnostics<monostate>
}
to_boolean(MutatingFunction) ::
  (node, T*, state) -> with_diagnostics<monostate>
```
 *
 * A value-producing callback's T* adapter requires assignment from its result.
 * A pointer-only callback provides no type<T> overload. Callbacks may return
 * with_diagnostics instead of expected. Expected errors use node-relative
 * paths; with_diagnostics carries complete paths and configuration.
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
ValueFunction :: (node, state) -> Result<T>
ValueFunction :: (node, type<T>) -> Result<T>
ValueFunction :: (node, type<T>, state) -> Result<T>
MutatingFunction :: (node, T*) -> Result<monostate>
MutatingFunction :: (node, T*, state) -> Result<monostate>

to_integer(ValueFunction) :: {
  (node, type<T>, state) -> with_diagnostics<T>,
  (node, T*, state)      -> with_diagnostics<monostate>
}
to_integer(MutatingFunction) ::
  (node, T*, state) -> with_diagnostics<monostate>
```
 *
 * A value-producing callback's T* adapter requires assignment from its result.
 * A pointer-only callback provides no type<T> overload. Callbacks may return
 * with_diagnostics instead of expected. Expected errors use node-relative
 * paths; with_diagnostics carries complete paths and configuration.
 * Conversion and destination range checks belong to f.
 * The compositor does not call reflected_scope::mapping_to_type.
 * @pre The node belongs to a valid tree; destination pointers are non-null. */
template <typename Function>
constexpr auto to_integer(Function &&f)
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
ValueFunction :: (node, state) -> Result<T>
ValueFunction :: (node, type<T>) -> Result<T>
ValueFunction :: (node, type<T>, state) -> Result<T>
MutatingFunction :: (node, T*) -> Result<monostate>
MutatingFunction :: (node, T*, state) -> Result<monostate>

to_real(ValueFunction) :: {
  (node, type<T>, state) -> with_diagnostics<T>,
  (node, T*, state)      -> with_diagnostics<monostate>
}
to_real(MutatingFunction) ::
  (node, T*, state) -> with_diagnostics<monostate>
```
 *
 * A value-producing callback's T* adapter requires assignment from its result.
 * A pointer-only callback provides no type<T> overload. Callbacks may return
 * with_diagnostics instead of expected. Expected errors use node-relative
 * paths; with_diagnostics carries complete paths and configuration.
 * Conversion and destination range checks belong to f.
 * The compositor does not call reflected_scope::mapping_to_type.
 * @pre The node belongs to a valid tree; destination pointers are non-null. */
template <typename Function>
constexpr auto to_real(Function &&f)
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
ValueFunction :: (node, state) -> Result<T>
ValueFunction :: (node, type<T>) -> Result<T>
ValueFunction :: (node, type<T>, state) -> Result<T>
MutatingFunction :: (node, T*) -> Result<monostate>
MutatingFunction :: (node, T*, state) -> Result<monostate>

to_arithmetic(ValueFunction) :: {
  (node, type<T>, state) -> with_diagnostics<T>,
  (node, T*, state)      -> with_diagnostics<monostate>
}
to_arithmetic(MutatingFunction) ::
  (node, T*, state) -> with_diagnostics<monostate>
```
 *
 * A value-producing callback's T* adapter requires assignment from its result.
 * A pointer-only callback provides no type<T> overload. Callbacks may return
 * with_diagnostics instead of expected. Expected errors use node-relative
 * paths; with_diagnostics carries complete paths and configuration.
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
ValueFunction :: (node, state) -> Result<T>
ValueFunction :: (node, type<T>) -> Result<T>
ValueFunction :: (node, type<T>, state) -> Result<T>
MutatingFunction :: (node, T*) -> Result<monostate>
MutatingFunction :: (node, T*, state) -> Result<monostate>

to_optional_like(ValueFunction) :: {
  (node, type<T>, state) -> with_diagnostics<T>,
  (node, T*, state)      -> with_diagnostics<monostate>
}
to_optional_like(MutatingFunction) ::
  (node, T*, state) -> with_diagnostics<monostate>
```
 *
 * A value-producing callback's T* adapter requires assignment from its result.
 * A pointer-only callback provides no type<T> overload. Callbacks may return
 * with_diagnostics instead of expected. Expected errors use node-relative
 * paths; with_diagnostics carries complete paths and configuration.
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

namespace detail {

/** Calls a node check before the mapping function, preserving its overloads.
 * A failed check returns one hard error at the current path.
 * The mapping function is skipped on failure or an exceeded diagnostic limit.
 *
```
Check :: node -> optional<issue>
Mapping :: (node, destination, state) -> with_diagnostics<T>

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
  template <typename Destination, typename State>
  auto operator()(::ryml::ConstNodeRef node, Destination into,
    State state) const -> decltype(this->lifted_mapping(node, into, state)) {
    if (state.limit_exceeded())
      return {
        /*value=*/compat::nullopt,
        /*diagnostics=*/{/*config=*/{state.config.error_policy, {}}, {}, {}},
      };

    if (auto check = this->expect_node(node)) {
      auto path = state.path.materialize();
      path.insert(path.end(), check->path.begin(), check->path.end());
      check->path = std::move(path);
      return {
        /*value=*/compat::nullopt,
        /*diagnostics=*/{
          /*config=*/{state.config.error_policy, {}},
          /*issues=*/{std::move(*check)},
          /*parse_error_message=*/{},
        },
      };
    }

    return this->lifted_mapping(node, into, state);
  }
};

template <typename Mapping>
struct with_mutating_node_to_type_overload_t {
  Mapping mapping;

  // Overload ranking preserves a native pointer call before trying assignment.
  template <typename T, typename State>
  auto map_into(::ryml::ConstNodeRef node, T *into, State state, int) const
    -> decltype(this->mapping(node, into, state)) {
    return this->mapping(node, into, state);
  }

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_assignable<T &, T &&>::value))
  auto map_into(::ryml::ConstNodeRef node, T *into, State state, long) const
    -> with_diagnostics_t<compat::monostate, compat::decay_t<decltype(
      this->mapping(node, omni::type_t<T>{}, state).diagnostics)>> {
    auto result = this->mapping(node, omni::type_t<T>{}, state);
    if (result.value)
      *into = std::move(*result.value);

    return {
      /*value=*/result.value
        ? compat::make_optional(compat::monostate{}) : compat::nullopt,
      /*diagnostics=*/std::move(result.diagnostics),
    };
  }

  // The trailing decltype excludes unsupported wrapped calls through SFINAE.
  template <typename T, typename State>
  auto operator()(::ryml::ConstNodeRef node, omni::type_t<T> type,
    State state) const -> decltype(this->mapping(node, type, state)) {
    return this->mapping(node, type, state);
  }

  template <typename T, typename State>
  auto operator()(::ryml::ConstNodeRef node, T *into, State state) const
    -> decltype(this->map_into(node, into, state, 0)) {
    assert(into);
    return this->map_into(node, into, state, 0);
  }
};

} // namespace detail
} // namespace compose
} // namespace ryml
} // namespace omni

#include <omnirefl/serialization/ryml/impl/compose.hpp>
