#pragma once

/** Internal callback signature and result adaptation. */

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/functional.hpp>
#include <omnirefl/serialization/ryml/core.hpp>
#include <omnirefl/serialization/ryml/state.hpp>
#include <omnirefl/serialization/ryml/with_diagnostics.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <cassert>
#include <iterator>
#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {
namespace detail {

// Prefer full arguments, then pointer-only, state-only, and node-only.
template <typename Function, typename Destination, typename State>
auto invoke_expected_node(State state,
  const Function &f,
  ::ryml::ConstNodeRef node,
  Destination into,
  node_call_priority<3>)
  -> decltype(f(std::move(state), node, into)) {
  return f(std::move(state), node, into);
}

template <typename Function, typename T, typename State>
auto invoke_expected_node(State,
  const Function &f,
  ::ryml::ConstNodeRef node,
  T *into,
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

} // namespace detail

namespace compose {
namespace detail {

template <typename Function>
struct lifted_node_function final {
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
