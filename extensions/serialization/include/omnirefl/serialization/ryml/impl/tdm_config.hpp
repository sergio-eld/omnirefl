#pragma once

/** Functional data-member update providers; owners remain aggregates from
 * C++17 onward. Definitions precede the public default objects. */

#include <omnirefl/compat.hpp>

#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {

template <typename TransformDiagnostics>
struct config_t;

namespace detail {

template <typename TransformDiagnostics>
struct tdm_for_config {
  /** (QoL) Transform data member `.transform_diagnostics` inplace.
   * Configure it within a single expression.
   * Invokes f immediately with the current callable and returns a new config
   * containing its result, preserving error_policy and this config.
   * f :: TransformDiagnostics -> NewTransformDiagnostics.
   *
```cpp
const auto load = omni::ryml::deserialize.tdm_config(
  [](omni::ryml::config_t<> options) {
    return options.tdm_transform_diagnostics(
      [](omni::ryml::render_diagnostics_t previous) {
        return [previous](const ::ryml::Tree &tree,
          omni::ryml::diagnostics report) {
          return previous(tree, report).size();
        };
      });
  });
``` */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, const TransformDiagnostics &>::value))
  constexpr auto tdm_transform_diagnostics(F &&f) const &
    -> config_t<compat::decay_t<
      compat::invoke_result_t<F &&, const TransformDiagnostics &>>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      this->derived().error_policy,
      /*transform_diagnostics=*/compat::invoke(
        std::forward<F>(f), this->derived().transform_diagnostics),
    };
  }

  /** Transform `.transform_diagnostics`, consuming this configuration. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, TransformDiagnostics &&>::value))
  // C++11 constexpr member functions are implicitly const, preventing moves.
  OMNI_CPP14_CONSTEXPR auto tdm_transform_diagnostics(F &&f) &&
    -> config_t<compat::decay_t<
      compat::invoke_result_t<F &&, TransformDiagnostics &&>>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      std::move(this->derived().error_policy),
      /*transform_diagnostics=*/compat::invoke(std::forward<F>(f),
        std::move(this->derived().transform_diagnostics)),
    };
  }

private:
  // Direct returns keep the updater bodies constexpr-compatible with C++11.
  constexpr const config_t<TransformDiagnostics> &derived() const & {
    return static_cast<const config_t<TransformDiagnostics> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR config_t<TransformDiagnostics> &derived() & {
    return static_cast<config_t<TransformDiagnostics> &>(*this);
  }
};

} // namespace detail
} // namespace ryml
} // namespace omni
