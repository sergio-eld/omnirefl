#pragma once

/** Terminal mapping overloads; no recursive mapping is instantiated. */

#include <omnirefl/serialization/ryml/state.hpp>

namespace omni {
namespace ryml {
namespace detail {

inline diagnostics depth_error(::ryml::ConstNodeRef node);

struct with_depth_exceeded_operator {
  template <typename T>
  auto operator()(const depth_exceeded &, ::ryml::ConstNodeRef node,
    omni::type_t<T>) const -> with_diagnostics_t<T> {
    return {compat::nullopt, depth_error(node)};
  }

  template <typename T>
  auto operator()(const depth_exceeded &stopped, ::ryml::ConstNodeRef node,
    T *) const -> with_diagnostics_t<compat::monostate> {
    return (*this)(stopped, node, omni::type_t<compat::monostate>{});
  }

  // FIXME: [high] Return a diagnostic for write-depth overflow. The current
  // void write pipeline drops this error and may leave incomplete output.
  template <typename T>
  void operator()(const depth_exceeded &, const T &, ::ryml::NodeRef) const {}
};

} // namespace detail
} // namespace ryml
} // namespace omni
