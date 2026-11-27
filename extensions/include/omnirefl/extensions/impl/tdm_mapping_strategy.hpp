#pragma once

/** Functional data-member update providers; owners remain aggregates from
 * C++17 onward. Definitions precede the public default objects. */

#include <omnirefl/compat.hpp>

#include <type_traits>
#include <utility>

namespace omni {
namespace mapping {

template <typename FilterFields, typename AliasesByName,
  typename ValidateFields, typename Finalize, typename KeepNulls>
struct record_strategy_t;

template <typename AliasesByName>
struct enum_strategy_t;

namespace detail {

template <typename FilterFields, typename AliasesByName,
  typename ValidateFields, typename Finalize, typename KeepNulls>
struct record_strategy_updates {
  /** (QoL) Functional update of .filter_fields; other members are
   * copied from the const strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, const FilterFields &>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value
      && std::is_constructible<AliasesByName, const AliasesByName &>::value
      && std::is_constructible<ValidateFields, const ValidateFields &>::value
      && std::is_constructible<Finalize, const Finalize &>::value))
  constexpr auto tdm_filter_fields(F &&f) const &
    -> record_strategy_t<compat::decay_t<Result>,
      AliasesByName,
      ValidateFields,
      Finalize, KeepNulls> {
    return make_strategy(
      compat::invoke(std::forward<F>(f), this->derived().filter_fields),
      this->derived().aliases_by_name,
      this->derived().validate_fields,
      this->derived().finalize,
      this->derived().keep_nulls);
  }

  /** (QoL) Functional update of .filter_fields; other members are
   * moved from the consumed strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, FilterFields &&>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value
      && std::is_constructible<AliasesByName, AliasesByName &&>::value
      && std::is_constructible<ValidateFields, ValidateFields &&>::value
      && std::is_constructible<Finalize, Finalize &&>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_filter_fields(F &&f) &&
    -> record_strategy_t<compat::decay_t<Result>,
      AliasesByName,
      ValidateFields,
      Finalize, KeepNulls> {
    return make_strategy(
      compat::invoke(std::forward<F>(f),
        std::move(this->derived().filter_fields)),
      std::move(this->derived().aliases_by_name),
      std::move(this->derived().validate_fields),
      std::move(this->derived().finalize),
      std::move(this->derived().keep_nulls));
  }

  /** (QoL) Functional update of .aliases_by_name; other members are
   * copied from the const strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, const AliasesByName &>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value
      && std::is_constructible<FilterFields, const FilterFields &>::value
      && std::is_constructible<ValidateFields, const ValidateFields &>::value
      && std::is_constructible<Finalize, const Finalize &>::value))
  constexpr auto tdm_aliases_by_name(F &&f) const &
    -> record_strategy_t<FilterFields,
      compat::decay_t<Result>,
      ValidateFields,
      Finalize, KeepNulls> {
    return make_strategy(
      this->derived().filter_fields,
      compat::invoke(std::forward<F>(f), this->derived().aliases_by_name),
      this->derived().validate_fields,
      this->derived().finalize,
      this->derived().keep_nulls);
  }

  /** (QoL) Functional update of .aliases_by_name; other members are
   * moved from the consumed strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, AliasesByName &&>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value
      && std::is_constructible<FilterFields, FilterFields &&>::value
      && std::is_constructible<ValidateFields, ValidateFields &&>::value
      && std::is_constructible<Finalize, Finalize &&>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_aliases_by_name(F &&f) &&
    -> record_strategy_t<FilterFields,
      compat::decay_t<Result>,
      ValidateFields,
      Finalize, KeepNulls> {
    return make_strategy(
      std::move(this->derived().filter_fields),
      compat::invoke(std::forward<F>(f),
        std::move(this->derived().aliases_by_name)),
      std::move(this->derived().validate_fields),
      std::move(this->derived().finalize),
      std::move(this->derived().keep_nulls));
  }

  /** (QoL) Functional update of .validate_fields; other members are
   * copied from the const strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, const ValidateFields &>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value
      && std::is_constructible<FilterFields, const FilterFields &>::value
      && std::is_constructible<AliasesByName, const AliasesByName &>::value
      && std::is_constructible<Finalize, const Finalize &>::value))
  constexpr auto tdm_validate_fields(F &&f) const &
    -> record_strategy_t<FilterFields,
      AliasesByName,
      compat::decay_t<Result>,
      Finalize, KeepNulls> {
    return make_strategy(
      this->derived().filter_fields,
      this->derived().aliases_by_name,
      compat::invoke(std::forward<F>(f), this->derived().validate_fields),
      this->derived().finalize,
      this->derived().keep_nulls);
  }

  /** (QoL) Functional update of .validate_fields; other members are
   * moved from the consumed strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, ValidateFields &&>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value
      && std::is_constructible<FilterFields, FilterFields &&>::value
      && std::is_constructible<AliasesByName, AliasesByName &&>::value
      && std::is_constructible<Finalize, Finalize &&>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_validate_fields(F &&f) &&
    -> record_strategy_t<FilterFields,
      AliasesByName,
      compat::decay_t<Result>,
      Finalize, KeepNulls> {
    return make_strategy(
      std::move(this->derived().filter_fields),
      std::move(this->derived().aliases_by_name),
      compat::invoke(std::forward<F>(f),
        std::move(this->derived().validate_fields)),
      std::move(this->derived().finalize),
      std::move(this->derived().keep_nulls));
  }

  /** (QoL) Functional update of .finalize; other members are
   * copied from the const strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, const Finalize &>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value
      && std::is_constructible<FilterFields, const FilterFields &>::value
      && std::is_constructible<AliasesByName, const AliasesByName &>::value
      && std::is_constructible<ValidateFields, const ValidateFields &>::value))
  constexpr auto tdm_finalize(F &&f) const &
    -> record_strategy_t<FilterFields,
      AliasesByName,
      ValidateFields,
      compat::decay_t<Result>, KeepNulls> {
    return make_strategy(
      this->derived().filter_fields,
      this->derived().aliases_by_name,
      this->derived().validate_fields,
      compat::invoke(std::forward<F>(f), this->derived().finalize),
      this->derived().keep_nulls);
  }

  /** (QoL) Functional update of .finalize; other members are
   * moved from the consumed strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, Finalize &&>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value
      && std::is_constructible<FilterFields, FilterFields &&>::value
      && std::is_constructible<AliasesByName, AliasesByName &&>::value
      && std::is_constructible<ValidateFields, ValidateFields &&>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_finalize(F &&f) &&
    -> record_strategy_t<FilterFields,
      AliasesByName,
      ValidateFields,
      compat::decay_t<Result>, KeepNulls> {
    return make_strategy(
      std::move(this->derived().filter_fields),
      std::move(this->derived().aliases_by_name),
      std::move(this->derived().validate_fields),
      compat::invoke(std::forward<F>(f), std::move(this->derived().finalize)),
      std::move(this->derived().keep_nulls));
  }

  /** (QoL) Functional update of .keep_nulls; other members are copied. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, const KeepNulls &>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value
      && std::is_constructible<FilterFields, const FilterFields &>::value
      && std::is_constructible<AliasesByName, const AliasesByName &>::value
      && std::is_constructible<ValidateFields, const ValidateFields &>::value
      && std::is_constructible<Finalize, const Finalize &>::value))
  constexpr auto tdm_keep_nulls(F &&f) const &
    -> record_strategy_t<FilterFields, AliasesByName, ValidateFields,
      Finalize, compat::decay_t<Result>> {
    return make_strategy(
      this->derived().filter_fields,
      this->derived().aliases_by_name,
      this->derived().validate_fields,
      this->derived().finalize,
      compat::invoke(std::forward<F>(f), this->derived().keep_nulls));
  }

  /** (QoL) Functional update of .keep_nulls; other members are moved. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, KeepNulls &&>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value
      && std::is_constructible<FilterFields, FilterFields &&>::value
      && std::is_constructible<AliasesByName, AliasesByName &&>::value
      && std::is_constructible<ValidateFields, ValidateFields &&>::value
      && std::is_constructible<Finalize, Finalize &&>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_keep_nulls(F &&f) &&
    -> record_strategy_t<FilterFields, AliasesByName, ValidateFields,
      Finalize, compat::decay_t<Result>> {
    return make_strategy(
      std::move(this->derived().filter_fields),
      std::move(this->derived().aliases_by_name),
      std::move(this->derived().validate_fields),
      std::move(this->derived().finalize),
      compat::invoke(std::forward<F>(f),
        std::move(this->derived().keep_nulls)));
  }

private:
  constexpr const record_strategy_t<FilterFields, AliasesByName,
    ValidateFields, Finalize, KeepNulls> &derived() const & {
    return static_cast<const record_strategy_t<FilterFields, AliasesByName,
      ValidateFields, Finalize, KeepNulls> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR record_strategy_t<FilterFields, AliasesByName,
    ValidateFields, Finalize, KeepNulls> &derived() & {
    return static_cast<record_strategy_t<FilterFields, AliasesByName,
      ValidateFields, Finalize, KeepNulls> &>(*this);
  }

  template <typename Filter, typename Aliases, typename Validators,
    typename Finish, typename Nulls>
  static constexpr auto make_strategy(Filter &&filter, Aliases &&aliases,
    Validators &&validators, Finish &&finish, Nulls &&nulls)
    -> record_strategy_t<compat::decay_t<Filter>, compat::decay_t<Aliases>,
      compat::decay_t<Validators>, compat::decay_t<Finish>,
      compat::decay_t<Nulls>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*filter_fields=*/std::forward<Filter>(filter),
      /*aliases_by_name=*/std::forward<Aliases>(aliases),
      /*validate_fields=*/std::forward<Validators>(validators),
      /*finalize=*/std::forward<Finish>(finish),
      /*keep_nulls=*/std::forward<Nulls>(nulls),
    };
  }
};

template <typename AliasesByName>
struct tdm_for_enum_strategy {
  /** (QoL) Functional update of .aliases_by_name. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, const AliasesByName &>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  constexpr auto tdm_aliases_by_name(F &&f) const &
    -> enum_strategy_t<compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      compat::invoke(std::forward<F>(f), this->derived().aliases_by_name),
    };
  }

  /** (QoL) Functional update consuming .aliases_by_name. */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Result = compat::invoke_result_t<F &&, AliasesByName &&>),
    (std::is_constructible<compat::decay_t<Result>, Result>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_aliases_by_name(F &&f) &&
    -> enum_strategy_t<compat::decay_t<Result>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      compat::invoke(std::forward<F>(f),
        std::move(this->derived().aliases_by_name)),
    };
  }

private:
  // Direct returns keep the updater bodies constexpr-compatible with C++11.
  constexpr const enum_strategy_t<AliasesByName> &derived() const & {
    return static_cast<const enum_strategy_t<AliasesByName> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR enum_strategy_t<AliasesByName> &derived() & {
    return static_cast<enum_strategy_t<AliasesByName> &>(*this);
  }
};

} // namespace detail
} // namespace mapping
} // namespace omni
