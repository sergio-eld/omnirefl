#pragma once

/** Functional updates for range mappings. */

#include <omnirefl/compat.hpp>

#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {

template <typename Policy>
struct map_range_t;

namespace detail {

template <typename Policy>
struct tdm_for_map_range {
  /** (QoL) Functional update of the shared mapping policy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, const Policy &>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  constexpr auto tdm_policy(F &&f) const &
    -> map_range_t<compat::decay_t<Result>> {
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
    -> map_range_t<compat::decay_t<Result>> {
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
  constexpr const map_range_t<Policy> &derived() const & {
    return static_cast<const map_range_t<Policy> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR map_range_t<Policy> &derived() & {
    return static_cast<map_range_t<Policy> &>(*this);
  }
};

} // namespace detail
} // namespace ryml
} // namespace omni
