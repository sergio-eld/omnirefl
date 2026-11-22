#pragma once

/** Functional updates of built-in node-check toggles. */

#include <omnirefl/compat.hpp>

#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {

template <typename WithNodeCheck>
struct map_string_node_t;

template <typename WithNodeCheck>
struct map_boolean_node_t;

template <typename WithNodeCheck>
struct map_integer_node_t;

template <typename WithNodeCheck>
struct map_number_node_t;

template <typename WithNodeCheck>
struct map_arithmetic_node_t;

template <typename WithNodeCheck>
struct map_range_node_t;

template <typename WithNodeCheck>
struct map_value_node_t;

namespace detail {

template <typename WithNodeCheck>
struct tdm_for_map_string_node {
  /** (QoL) Functional update of .with_node_check. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, const WithNodeCheck &>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  constexpr auto tdm_with_node_check(F &&f) const &
    -> map_string_node_t<compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      compat::invoke(std::forward<F>(f), this->derived().with_node_check),
    };
  }

  /** (QoL) Functional update consuming .with_node_check. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, WithNodeCheck &&>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_with_node_check(F &&f) &&
    -> map_string_node_t<compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      compat::invoke(std::forward<F>(f),
        std::move(this->derived().with_node_check)),
    };
  }

private:
  constexpr const map_string_node_t<WithNodeCheck> &derived() const & {
    return static_cast<const map_string_node_t<WithNodeCheck> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR map_string_node_t<WithNodeCheck> &derived() & {
    return static_cast<map_string_node_t<WithNodeCheck> &>(*this);
  }
};

template <typename WithNodeCheck>
struct tdm_for_map_boolean_node {
  /** (QoL) Functional update of .with_node_check. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, const WithNodeCheck &>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  constexpr auto tdm_with_node_check(F &&f) const &
    -> map_boolean_node_t<compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      compat::invoke(std::forward<F>(f), this->derived().with_node_check),
    };
  }

  /** (QoL) Functional update consuming .with_node_check. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, WithNodeCheck &&>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_with_node_check(F &&f) &&
    -> map_boolean_node_t<compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      compat::invoke(std::forward<F>(f),
        std::move(this->derived().with_node_check)),
    };
  }

private:
  constexpr const map_boolean_node_t<WithNodeCheck> &derived() const & {
    return static_cast<const map_boolean_node_t<WithNodeCheck> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR map_boolean_node_t<WithNodeCheck> &derived() & {
    return static_cast<map_boolean_node_t<WithNodeCheck> &>(*this);
  }
};

template <typename WithNodeCheck>
struct tdm_for_map_integer_node {
  /** (QoL) Functional update of .with_node_check. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, const WithNodeCheck &>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  constexpr auto tdm_with_node_check(F &&f) const &
    -> map_integer_node_t<compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      compat::invoke(std::forward<F>(f), this->derived().with_node_check),
    };
  }

  /** (QoL) Functional update consuming .with_node_check. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, WithNodeCheck &&>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_with_node_check(F &&f) &&
    -> map_integer_node_t<compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      compat::invoke(std::forward<F>(f),
        std::move(this->derived().with_node_check)),
    };
  }

private:
  constexpr const map_integer_node_t<WithNodeCheck> &derived() const & {
    return static_cast<const map_integer_node_t<WithNodeCheck> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR map_integer_node_t<WithNodeCheck> &derived() & {
    return static_cast<map_integer_node_t<WithNodeCheck> &>(*this);
  }
};

template <typename WithNodeCheck>
struct tdm_for_map_number_node {
  /** (QoL) Functional update of .with_node_check. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, const WithNodeCheck &>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  constexpr auto tdm_with_node_check(F &&f) const &
    -> map_number_node_t<compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      compat::invoke(std::forward<F>(f), this->derived().with_node_check),
    };
  }

  /** (QoL) Functional update consuming .with_node_check. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, WithNodeCheck &&>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_with_node_check(F &&f) &&
    -> map_number_node_t<compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      compat::invoke(std::forward<F>(f),
        std::move(this->derived().with_node_check)),
    };
  }

private:
  constexpr const map_number_node_t<WithNodeCheck> &derived() const & {
    return static_cast<const map_number_node_t<WithNodeCheck> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR map_number_node_t<WithNodeCheck> &derived() & {
    return static_cast<map_number_node_t<WithNodeCheck> &>(*this);
  }
};

template <typename WithNodeCheck>
struct tdm_for_map_arithmetic_node {
  /** (QoL) Functional update of .with_node_check. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, const WithNodeCheck &>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  constexpr auto tdm_with_node_check(F &&f) const &
    -> map_arithmetic_node_t<compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      compat::invoke(std::forward<F>(f), this->derived().with_node_check),
    };
  }

  /** (QoL) Functional update consuming .with_node_check. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, WithNodeCheck &&>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_with_node_check(F &&f) &&
    -> map_arithmetic_node_t<compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      compat::invoke(std::forward<F>(f),
        std::move(this->derived().with_node_check)),
    };
  }

private:
  constexpr const map_arithmetic_node_t<WithNodeCheck> &derived() const & {
    return static_cast<const map_arithmetic_node_t<WithNodeCheck> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR map_arithmetic_node_t<WithNodeCheck> &derived() & {
    return static_cast<map_arithmetic_node_t<WithNodeCheck> &>(*this);
  }
};

template <typename WithNodeCheck>
struct tdm_for_map_range_node {
  /** (QoL) Functional update of .with_node_check. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, const WithNodeCheck &>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  constexpr auto tdm_with_node_check(F &&f) const &
    -> map_range_node_t<compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      compat::invoke(std::forward<F>(f), this->derived().with_node_check),
    };
  }

  /** (QoL) Functional update consuming .with_node_check. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, WithNodeCheck &&>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_with_node_check(F &&f) &&
    -> map_range_node_t<compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      compat::invoke(std::forward<F>(f),
        std::move(this->derived().with_node_check)),
    };
  }

private:
  constexpr const map_range_node_t<WithNodeCheck> &derived() const & {
    return static_cast<const map_range_node_t<WithNodeCheck> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR map_range_node_t<WithNodeCheck> &derived() & {
    return static_cast<map_range_node_t<WithNodeCheck> &>(*this);
  }
};

template <typename WithNodeCheck>
struct tdm_for_map_value_node {
  /** (QoL) Functional update of .with_node_check. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, const WithNodeCheck &>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  constexpr auto tdm_with_node_check(F &&f) const &
    -> map_value_node_t<compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      compat::invoke(std::forward<F>(f), this->derived().with_node_check),
    };
  }

  /** (QoL) Functional update consuming .with_node_check. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, WithNodeCheck &&>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_with_node_check(F &&f) &&
    -> map_value_node_t<compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*tdm_base=*/{},
      /*depth_base=*/{},
#endif
      compat::invoke(std::forward<F>(f),
        std::move(this->derived().with_node_check)),
    };
  }

private:
  constexpr const map_value_node_t<WithNodeCheck> &derived() const & {
    return static_cast<const map_value_node_t<WithNodeCheck> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR map_value_node_t<WithNodeCheck> &derived() & {
    return static_cast<map_value_node_t<WithNodeCheck> &>(*this);
  }
};

} // namespace detail
} // namespace ryml
} // namespace omni
