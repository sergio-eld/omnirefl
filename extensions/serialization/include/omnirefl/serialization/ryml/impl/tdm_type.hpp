#pragma once

/** Functional updates for range mappings. */

#include <omnirefl/compat.hpp>

#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {

template <typename KeepNulls>
struct map_range_t;

namespace detail {

template <typename KeepNulls>
struct tdm_for_map_range {
  /** (QoL) Functional update of `.keep_nulls`. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, const KeepNulls &>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  constexpr auto tdm_keep_nulls(F &&f) const &
    -> map_range_t<compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      compat::invoke(std::forward<F>(f), this->derived().keep_nulls),
    };
  }

  /** (QoL) Functional update consuming `.keep_nulls`. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, KeepNulls &&>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_keep_nulls(F &&f) &&
    -> map_range_t<compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      compat::invoke(std::forward<F>(f),
        std::move(this->derived().keep_nulls)),
    };
  }

private:
  constexpr const map_range_t<KeepNulls> &derived() const & {
    return static_cast<const map_range_t<KeepNulls> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR map_range_t<KeepNulls> &derived() & {
    return static_cast<map_range_t<KeepNulls> &>(*this);
  }
};

} // namespace detail
} // namespace ryml
} // namespace omni
