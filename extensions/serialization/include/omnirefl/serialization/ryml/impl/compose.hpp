#pragma once

/** Internal callback signature and result adaptation. */

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/functional.hpp>
#include <omnirefl/serialization/ryml/impl/shared.hpp>
#include <omnirefl/serialization/ryml/state.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <cassert>
#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {
namespace detail {

// Prefer full arguments, then destination-only, state-only, and node-only.
template <int Rank>
struct node_call_priority: node_call_priority<Rank - 1> {};

template <>
struct node_call_priority<0> {};

template <typename Function, typename Destination, typename State>
auto invoke_expected_node(State state,
  const Function &f,
  ::ryml::ConstNodeRef node,
  Destination into,
  node_call_priority<3>)
  -> decltype(f(std::move(state), node, into)) {
  return f(std::move(state), node, into);
}

template <typename Function, typename Destination, typename State>
auto invoke_expected_node(State,
  const Function &f,
  ::ryml::ConstNodeRef node,
  Destination into,
  node_call_priority<2>) -> decltype(f(node, into)) {
  return f(node, into);
}

template <typename Function, typename T, typename State>
auto invoke_expected_node(State state,
  const Function &f,
  ::ryml::ConstNodeRef node,
  omni::type_t<T>,
  node_call_priority<1>)
  -> decltype(f(std::move(state), node)) {
  return f(std::move(state), node);
}

template <typename Function, typename T, typename State>
auto invoke_expected_node(State,
  const Function &f,
  ::ryml::ConstNodeRef node,
  omni::type_t<T>,
  node_call_priority<0>) -> decltype(f(node)) {
  return f(node);
}

template <typename Function, typename Destination, typename State>
auto invoke_expected_node(State state,
  const Function &f,
  ::ryml::ConstNodeRef node,
  Destination into)
  -> decltype(invoke_expected_node(std::move(state),
    f,
    node,
    into,
    node_call_priority<3>{})) {
  return invoke_expected_node(std::move(state),
    f,
    node,
    into,
    node_call_priority<3>{});
}

inline issue node_function_error(issue error, ::ryml::ConstNodeRef node) {
  if (error.node_id == ::ryml::NONE) {
    auto prefix = core::node_path(node);
    prefix.insert(prefix.end(), error.path.begin(), error.path.end());
    error.path = std::move(prefix);
    error.node_id = node.id();
  }

  const ::ryml::ConstNodeRef source{node.tree(), error.node_id};
  if (error.path.empty())
    error.path = core::node_path(source);

  if (error.actual == json::value_kind::unknown)
    error.actual = node_kind(source);

  return error;
}

template <typename T, typename State>
auto node_result(State state,
  compat::expected<T, issue> mapped,
  ::ryml::ConstNodeRef node) -> with_diagnostics_t<T> {
  auto result = make_node_accumulator(std::move(state));
  if (mapped)
    return {std::move(*mapped), std::move(result.result.diagnostics)};

  result.report(node_function_error(std::move(mapped.error()), node));

  return {compat::nullopt, std::move(result.result.diagnostics)};
}

OMNI_CONSTRAINED_TEMPLATE((typename T, typename Errors, typename State,
  typename Error = compat::remove_cvref_t<compat::range_reference_t<Errors>>),
  (compat::is_input_range<Errors>::value && std::is_same<issue, Error>::value))
auto node_result(State state,
  compat::expected<T, Errors> mapped,
  ::ryml::ConstNodeRef node) -> with_diagnostics_t<T> {
  auto result = make_node_accumulator(std::move(state));
  if (mapped)
    return {std::move(*mapped), std::move(result.result.diagnostics)};

  for (auto &&error : mapped.error()) {
    result.report(node_function_error(std::move(error), node));
    if (result.stopped())
      break;
  }

  return {compat::nullopt, std::move(result.result.diagnostics)};
}

template <typename T, typename State>
auto node_result(State, with_diagnostics_t<T> mapped, ::ryml::ConstNodeRef)
  -> with_diagnostics_t<T> {
  return mapped;
}

} // namespace detail

namespace compose {
namespace detail {

template <typename Function>
struct lifted_node_function {
  Function function;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State,
    typename Result = decltype(omni::ryml::detail::invoke_expected_node(
      std::declval<State>(),
      std::declval<const Function &>(),
      std::declval<::ryml::ConstNodeRef>(),
      omni::type_t<T>{}))),
    (std::is_same<with_diagnostics_t<T>, decltype(
      omni::ryml::detail::node_result(std::declval<State>(),
        std::declval<Result>(),
        std::declval<::ryml::ConstNodeRef>()))>::value
      && is_read_state<State>::value))
  auto operator()(State state,
    ::ryml::ConstNodeRef node,
    omni::type_t<T> type) const -> with_diagnostics_t<T> {
    // The callback receives a snapshot; normalization still needs this state.
    auto mapped = omni::ryml::detail::invoke_expected_node(state,
      this->function,
      node,
      type);

    return omni::ryml::detail::node_result(std::move(state),
      std::move(mapped),
      node);
  }

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State,
    typename Result = decltype(omni::ryml::detail::invoke_expected_node(
      std::declval<State>(),
      std::declval<const Function &>(),
      std::declval<::ryml::ConstNodeRef>(),
      std::declval<T *>()))),
    (std::is_same<with_diagnostics_t<compat::monostate>, decltype(
      omni::ryml::detail::node_result(std::declval<State>(),
        std::declval<Result>(),
        std::declval<::ryml::ConstNodeRef>()))>::value
      && is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate> {
    assert(into);
    // The callback receives a snapshot; normalization still needs this state.
    auto mapped = omni::ryml::detail::invoke_expected_node(state,
      this->function,
      node,
      into);

    return omni::ryml::detail::node_result(std::move(state),
      std::move(mapped),
      node);
  }
};

} // namespace detail
} // namespace compose
} // namespace ryml
} // namespace omni
