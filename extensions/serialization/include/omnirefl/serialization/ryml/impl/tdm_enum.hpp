#pragma once

/** Functional data-member update providers; owners remain aggregates from
 * C++17 onward. Definitions precede the public default objects. */

#include <omnirefl/compat.hpp>

#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {
namespace core {
namespace reflected_scope {

template <typename Strategy>
struct map_node_to_enum_t;

namespace detail {

template <typename Strategy>
struct tdm_for_map_node_to_enum {
  /** (QoL) Functional update of the shared mapping strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, const Strategy &>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  constexpr auto tdm_strategy(F &&f) const &
    -> map_node_to_enum_t<compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      compat::invoke(std::forward<F>(f), this->derived().strategy),
    };
  }

  /** (QoL) Functional update consuming the bound strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, Strategy &&>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_strategy(F &&f) &&
    -> map_node_to_enum_t<compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      compat::invoke(std::forward<F>(f),
        std::move(this->derived().strategy)),
    };
  }

private:
  // Direct returns keep the updater bodies constexpr-compatible with C++11.
  constexpr const map_node_to_enum_t<Strategy> &derived() const & {
    return static_cast<const map_node_to_enum_t<Strategy> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR map_node_to_enum_t<Strategy> &derived() & {
    return static_cast<map_node_to_enum_t<Strategy> &>(*this);
  }
};

} // namespace detail
} // namespace reflected_scope
} // namespace core
} // namespace ryml
} // namespace omni

namespace omni {
namespace ryml {
namespace reflected_scope {

template <typename Strategy>
struct map_enum_t;

namespace detail {

template <typename Strategy>
struct tdm_for_map_enum {
  /** (QoL) Functional update of the shared mapping strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, const Strategy &>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  constexpr auto tdm_strategy(F &&f) const &
    -> map_enum_t<compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      compat::invoke(std::forward<F>(f), this->derived().strategy),
    };
  }

  /** (QoL) Functional update consuming the bound strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, Strategy &&>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_strategy(F &&f) &&
    -> map_enum_t<compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      compat::invoke(std::forward<F>(f),
        std::move(this->derived().strategy)),
    };
  }

private:
  // Direct returns keep the updater bodies constexpr-compatible with C++11.
  constexpr const map_enum_t<Strategy> &derived() const & {
    return static_cast<const map_enum_t<Strategy> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR map_enum_t<Strategy> &derived() & {
    return static_cast<map_enum_t<Strategy> &>(*this);
  }
};

} // namespace detail
} // namespace reflected_scope
} // namespace ryml
} // namespace omni
