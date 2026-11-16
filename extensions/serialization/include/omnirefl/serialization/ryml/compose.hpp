#pragma once

/** Composition of node-to-type mapping functions. */

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/serialization/ryml/node_checks.hpp>
#include <omnirefl/serialization/ryml/state.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <cassert>
#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {
namespace detail {

// Expected callbacks can return an issue or a standard mapping error code.
// The two forms need different construction, not different public overloads.
template <typename Error>
struct node_function_error_case {
  static issue make(Error error, ::ryml::ConstNodeRef node, mapping_path path) {
    auto prefix = path.materialize();
    prefix.insert(prefix.end(), error.path.begin(), error.path.end());
    error.path = std::move(prefix);
    if (error.node_id == ::ryml::NONE)
      error.node_id = node.id();

    if (error.actual == json::value_kind::unknown)
      error.actual = node_kind(node);

    return error;
  }
};

template <>
struct node_function_error_case<json::issue_code> {
  static issue make(json::issue_code error, ::ryml::ConstNodeRef node,
    mapping_path path) {
    return make_issue(error, json::value_kind::unknown, node, path);
  }
};

} // namespace detail

namespace compose {

/** Calls a node check before the mapping function, preserving its overloads.
 * Check diagnostics receive the current path and policy configuration.
 * The mapping function is skipped on failure or an exceeded diagnostic limit.
 *
```text
Check :: node -> with_diagnostics<monostate>
Mapping :: (node, destination, state) -> with_diagnostics<T>

with_node_check<Check> :: Mapping -> Mapping
```
 *
 * Check is retained in the callable type: default selection constructs the
 * factory's result type, which must preserve its configured check. */
template <with_diagnostics_t<compat::monostate> (*Check)(::ryml::ConstNodeRef),
  typename Mapping>
struct with_node_check_t {
  Mapping mapping;

  // The trailing decltype excludes unsupported wrapped calls through SFINAE.
  template <typename Destination, typename State>
  auto operator()(::ryml::ConstNodeRef node, Destination into,
    State state) const -> decltype(this->mapping(node, into, state)) {
    auto checked = Check(node);
    auto result = omni::ryml::detail::make_node_accumulator(state);
    for (auto &entry : checked.diagnostics.issues) {
      auto path = state.path.materialize();
      path.insert(path.end(), entry.path.begin(), entry.path.end());
      entry.path = std::move(path);
      result.report(std::move(entry));
    }

    if (!checked.value || !result.result.value || result.stopped())
      return {compat::nullopt, std::move(result.result.diagnostics)};

    auto mapped = this->mapping(node, into, result.state);
    result.append({
      /*value=*/mapped.value
        ? compat::make_optional(compat::monostate{}) : compat::nullopt,
      /*diagnostics=*/std::move(mapped.diagnostics),
    });

    if (!result.result.value)
      mapped.value.reset();

    return {std::move(mapped.value), std::move(result.result.diagnostics)};
  }
};

/** Composes a node-only check with a mapping function.
 * Check is a non-overloaded free function and can be named without &. */
template <with_diagnostics_t<compat::monostate> (*Check)(::ryml::ConstNodeRef),
  typename Mapping>
constexpr auto with_node_check(Mapping &&mapping)
  -> with_node_check_t<Check, compat::decay_t<Mapping>> {
  return {/*mapping=*/std::forward<Mapping>(mapping)};
}

/** Adapts a node-only expected function to the type/state mapping contract.
 * Error is issue or json::issue_code. Issue paths are relative to this node;
 * composition prefixes the current path and retains the source node identity.
 *
```text
Function :: node -> expected<T, Error>
from_expected_node_function :: Function ->
  ((node, type<T>, state) -> with_diagnostics<T>)
```
 */
template <typename Function>
struct from_expected_node_function_t {
  Function function;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (is_read_state<State>::value))
  auto operator()(::ryml::ConstNodeRef node, omni::type_t<T>, State state) const
    -> with_diagnostics_t<T> {
    using expected_type = compat::invoke_result_t<const Function &,
      ::ryml::ConstNodeRef>;
    static_assert(std::is_same<T, typename expected_type::value_type>::value,
      "Node function must return expected<T, Error> for the destination T");

    auto mapped = this->function(node);
    auto result = omni::ryml::detail::make_node_accumulator(state);
    if (mapped)
      return {std::move(*mapped), std::move(result.result.diagnostics)};

    result.report(omni::ryml::detail::node_function_error_case<
      typename expected_type::error_type>::make(
        std::move(mapped.error()), node, state.path));

    return {compat::nullopt, std::move(result.result.diagnostics)};
  }
};

/** Adds type/state arguments and diagnostics to a node-only expected
 * function. */
OMNI_CONSTRAINED_TEMPLATE((typename Function),
  (compat::is_invocable<const compat::decay_t<Function> &,
    ::ryml::ConstNodeRef>::value))
constexpr auto from_expected_node_function(Function &&function)
  -> from_expected_node_function_t<compat::decay_t<Function>> {
  using expected_type = compat::invoke_result_t<
    const compat::decay_t<Function> &, ::ryml::ConstNodeRef>;
  static_assert(compat::disjunction<
      std::is_same<typename expected_type::error_type, issue>,
      std::is_same<typename expected_type::error_type,
        json::issue_code>>::value,
    "Node function error must be issue or json::issue_code");

  return {/*function=*/std::forward<Function>(function)};
}

/** (QoL) Adds a mutating overload to a value-producing mapping function.
 * Preserves the value-producing overload. The mutating overload assigns
 * successfully mapped values and retains the mapping diagnostics.
 * @pre The destination pointer must be non-null. Wrapped mapping
 * preconditions still apply.
 * An empty result gives no guarantee about the destination's state.
 */
template <typename Mapping>
struct with_mutating_node_to_type_overload_t {
  Mapping mapping;

  // The trailing decltype also constrains this overload through SFINAE:
  // if the wrapped call is invalid, this overload is excluded from selection.
  template <typename T, typename State>
  auto operator()(::ryml::ConstNodeRef node, omni::type_t<T> type,
    State state) const -> decltype(this->mapping(node, type, state)) {
    return this->mapping(node, type, state);
  }

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_assignable<T &, T &&>::value))
  auto operator()(::ryml::ConstNodeRef node, T *into, State state) const
    -> with_diagnostics_t<compat::monostate, compat::decay_t<decltype(
      this->mapping(node, omni::type_t<T>{}, state).diagnostics)>> {
    assert(into);
    auto result = this->mapping(node, omni::type_t<T>{}, state);
    if (result.value)
      *into = std::move(*result.value);

    return {
      /*value=*/result.value
        ? compat::make_optional(compat::monostate{}) : compat::nullopt,
      /*diagnostics=*/std::move(result.diagnostics),
    };
  }
};

/** (QoL) Adds a mutating overload to a value-producing mapping function.
 *
```text
Mapping :: (node, type<T>, state) -> with_diagnostics<T>

with_mutating_node_to_type_overload :: Mapping -> {
  (node, type<T>, state) -> with_diagnostics<T>,
  (node, T*, state)      -> with_diagnostics<monostate>
}
```
 */
template <typename Mapping>
constexpr auto with_mutating_node_to_type_overload(Mapping &&mapping)
  -> with_mutating_node_to_type_overload_t<compat::decay_t<Mapping>> {
  return {/*mapping=*/std::forward<Mapping>(mapping)};
}

} // namespace compose
} // namespace ryml
} // namespace omni
