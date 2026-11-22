#pragma once

/** Functional data-member update providers; owners remain aggregates from
 * C++17 onward. Definitions precede the public default objects. */

#include <omnirefl/compat.hpp>

#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {
namespace reflected_scope {

template <typename Strategy, typename WithNodeCheck>
struct map_object_node_t;

template <typename Strategy>
struct map_record_t;

namespace detail {

template <typename Strategy, typename WithNodeCheck>
struct tdm_for_map_object_node {
  /** (QoL) Functional update of the shared mapping strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, const Strategy &>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value
      && std::is_copy_constructible<WithNodeCheck>::value))
  constexpr auto tdm_strategy(F &&f) const &
    -> map_object_node_t<compat::decay_t<Result>, WithNodeCheck> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      compat::invoke(std::forward<F>(f), this->derived().strategy),
      this->derived().with_node_check,
    };
  }

  /** (QoL) Functional update consuming the bound strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, Strategy &&>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value
      && std::is_move_constructible<WithNodeCheck>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_strategy(F &&f) &&
    -> map_object_node_t<compat::decay_t<Result>, WithNodeCheck> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      compat::invoke(std::forward<F>(f),
        std::move(this->derived().strategy)),
      std::move(this->derived().with_node_check),
    };
  }

  /** (QoL) Functional update of .with_node_check, copying the strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, const WithNodeCheck &>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value
      && std::is_copy_constructible<Strategy>::value))
  constexpr auto tdm_with_node_check(F &&f) const &
    -> map_object_node_t<Strategy, compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      this->derived().strategy,
      compat::invoke(std::forward<F>(f), this->derived().with_node_check),
    };
  }

  /** (QoL) Functional update of .with_node_check, moving the strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, WithNodeCheck &&>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value
      && std::is_move_constructible<Strategy>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_with_node_check(F &&f) &&
    -> map_object_node_t<Strategy, compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      std::move(this->derived().strategy),
      compat::invoke(std::forward<F>(f),
        std::move(this->derived().with_node_check)),
    };
  }

private:
  // Direct returns keep the updater bodies constexpr-compatible with C++11.
  constexpr const map_object_node_t<Strategy, WithNodeCheck> &
  derived() const & {
    return static_cast<const map_object_node_t<Strategy, WithNodeCheck> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR map_object_node_t<Strategy, WithNodeCheck> &derived() & {
    return static_cast<map_object_node_t<Strategy, WithNodeCheck> &>(*this);
  }
};

template <typename Strategy>
struct tdm_for_map_record {
  /** (QoL) Functional update of the shared mapping strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, const Strategy &>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  constexpr auto tdm_strategy(F &&f) const &
    -> map_record_t<compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      compat::invoke(std::forward<F>(f), this->derived().strategy),
    };
  }

  /** (QoL) Functional update consuming the bound strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, Strategy &&>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_strategy(F &&f) &&
    -> map_record_t<compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      compat::invoke(std::forward<F>(f),
        std::move(this->derived().strategy)),
    };
  }

private:
  // Direct returns keep the updater bodies constexpr-compatible with C++11.
  constexpr const map_record_t<Strategy> &derived() const & {
    return static_cast<const map_record_t<Strategy> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR map_record_t<Strategy> &derived() & {
    return static_cast<map_record_t<Strategy> &>(*this);
  }
};

} // namespace detail
} // namespace reflected_scope
} // namespace ryml
} // namespace omni
