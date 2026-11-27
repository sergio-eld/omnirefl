#pragma once

/** Functional data-member update providers; owners remain aggregates from
 * C++17 onward. Definitions precede the public default objects. */

#include <omnirefl/compat.hpp>

#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {
namespace reflected_scope {

template <typename Policy, typename WithNodeCheck>
struct map_node_to_enum_t;

template <typename Policy>
struct map_enum_t;

namespace detail {

template <typename Policy, typename WithNodeCheck>
struct tdm_for_map_node_to_enum {
  /** (QoL) Functional update of the shared mapping policy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, const Policy &>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value
      && std::is_copy_constructible<WithNodeCheck>::value))
  constexpr auto tdm_policy(F &&f) const &
    -> map_node_to_enum_t<compat::decay_t<Result>, WithNodeCheck> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      compat::invoke(std::forward<F>(f), this->derived().policy),
      this->derived().with_node_check,
    };
  }

  /** (QoL) Functional update consuming the bound policy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, Policy &&>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value
      && std::is_move_constructible<WithNodeCheck>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_policy(F &&f) &&
    -> map_node_to_enum_t<compat::decay_t<Result>, WithNodeCheck> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      compat::invoke(std::forward<F>(f),
        std::move(this->derived().policy)),
      std::move(this->derived().with_node_check),
    };
  }

  /** (QoL) Functional update of .with_node_check, copying the policy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, const WithNodeCheck &>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value
      && std::is_copy_constructible<Policy>::value))
  constexpr auto tdm_with_node_check(F &&f) const &
    -> map_node_to_enum_t<Policy, compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      this->derived().policy,
      compat::invoke(std::forward<F>(f), this->derived().with_node_check),
    };
  }

  /** (QoL) Functional update of .with_node_check, moving the policy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, WithNodeCheck &&>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value
      && std::is_move_constructible<Policy>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_with_node_check(F &&f) &&
    -> map_node_to_enum_t<Policy, compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      std::move(this->derived().policy),
      compat::invoke(std::forward<F>(f),
        std::move(this->derived().with_node_check)),
    };
  }

private:
  // Direct returns keep the updater bodies constexpr-compatible with C++11.
  constexpr const map_node_to_enum_t<Policy, WithNodeCheck> &
  derived() const & {
    return static_cast<
      const map_node_to_enum_t<Policy, WithNodeCheck> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR map_node_to_enum_t<Policy, WithNodeCheck> &
  derived() & {
    return static_cast<map_node_to_enum_t<Policy, WithNodeCheck> &>(*this);
  }
};

template <typename Policy>
struct tdm_for_map_enum {
  /** (QoL) Functional update of the shared mapping policy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, const Policy &>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  constexpr auto tdm_policy(F &&f) const &
    -> map_enum_t<compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      compat::invoke(std::forward<F>(f), this->derived().policy),
    };
  }

  /** (QoL) Functional update consuming the bound policy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, Policy &&>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_policy(F &&f) &&
    -> map_enum_t<compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      compat::invoke(std::forward<F>(f),
        std::move(this->derived().policy)),
    };
  }

private:
  // Direct returns keep the updater bodies constexpr-compatible with C++11.
  constexpr const map_enum_t<Policy> &derived() const & {
    return static_cast<const map_enum_t<Policy> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR map_enum_t<Policy> &derived() & {
    return static_cast<map_enum_t<Policy> &>(*this);
  }
};

} // namespace detail
} // namespace reflected_scope
} // namespace ryml
} // namespace omni
