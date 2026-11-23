#pragma once

/** Optional mapping values with retained diagnostics and result composition. */

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/serialization/ryml/state.hpp>
#include <omnirefl/traits.hpp>

#include <iterator>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace omni {
namespace ryml {
namespace detail {

// Diagnostics are always present, including warnings accompanying a value.
// Sequencing combines reports; other report types supply an explicit merger.
struct append_diagnostics_t {
  auto operator()(omni::ryml::diagnostics first,
    omni::ryml::diagnostics second) const -> omni::ryml::diagnostics {
    first.issues.insert(first.issues.end(),
      std::make_move_iterator(second.issues.begin()),
      std::make_move_iterator(second.issues.end()));

    if (!second.parse_error_message.empty()) {
      if (!first.parse_error_message.empty())
        first.parse_error_message += '\n';

      first.parse_error_message += second.parse_error_message;
    }

    return first;
  }

  std::string operator()(std::string first, std::string second) const {
    if (first.empty())
      return second;

    if (!second.empty()) {
      first += '\n';
      first += second;
    }

    return first;
  }

  template <typename Issue, typename Allocator>
  auto operator()(std::vector<Issue, Allocator> first,
    std::vector<Issue, Allocator> second) const
    -> std::vector<Issue, Allocator> {
    first.insert(first.end(), std::make_move_iterator(second.begin()),
      std::make_move_iterator(second.end()));
    return first;
  }
};

} // namespace detail

/**
 * Holds the deserialized value, if available, and diagnostics even on failure.
 * Diagnostics can hold issue records or a rendered message.
 */
template <typename T,
  typename Diagnostics = omni::ryml::diagnostics>
struct with_diagnostics_t {
  using value_type = T;
  using diagnostics_type = Diagnostics;

  // Available only when mapping succeeds.
  compat::optional<T> value;

  // Retained issue records, or the result of transforming them for display.
  Diagnostics diagnostics;

  /** Transforms an available value with f, retaining diagnostics.
   * An empty value stays empty. f must return a non-void value; references
   * are stored as values. Rvalue overloads move the retained diagnostics.
   *
```
transform :: (T -> U) -> with_diagnostics<U, Diagnostics>
```
   */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename U = compat::decay_t<compat::invoke_result_t<F &&, T &>>),
    (std::is_constructible<U, compat::invoke_result_t<F &&, T &>>::value
      && std::is_constructible<Diagnostics, Diagnostics &>::value))
  OMNI_CPP14_CONSTEXPR auto transform(F &&f) &
    -> with_diagnostics_t<U, Diagnostics> {
    return {
      /*value=*/this->value
        ? compat::make_optional(compat::invoke(std::forward<F>(f),
            *this->value))
        : compat::nullopt,
      /*diagnostics=*/this->diagnostics,
    };
  }

  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename U = compat::decay_t<compat::invoke_result_t<F &&, const T &>>),
    (std::is_constructible<U, compat::invoke_result_t<F &&, const T &>>::value
      && std::is_constructible<Diagnostics, const Diagnostics &>::value))
  OMNI_CPP14_CONSTEXPR auto transform(F &&f) const &
    -> with_diagnostics_t<U, Diagnostics> {
    return {
      /*value=*/this->value
        ? compat::make_optional(compat::invoke(std::forward<F>(f),
            *this->value))
        : compat::nullopt,
      /*diagnostics=*/this->diagnostics,
    };
  }

  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename U = compat::decay_t<compat::invoke_result_t<F &&, T &&>>),
    (std::is_constructible<U, compat::invoke_result_t<F &&, T &&>>::value
      && std::is_constructible<Diagnostics, Diagnostics &&>::value))
  OMNI_CPP14_CONSTEXPR auto transform(F &&f) &&
    -> with_diagnostics_t<U, Diagnostics> {
    return {
      /*value=*/this->value
        ? compat::make_optional(compat::invoke(std::forward<F>(f),
            std::move(*this->value)))
        : compat::nullopt,
      /*diagnostics=*/std::move(this->diagnostics),
    };
  }

  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename U = compat::decay_t<compat::invoke_result_t<F &&, const T &&>>),
    (std::is_constructible<U, compat::invoke_result_t<F &&, const T &&>>::value
      && std::is_constructible<Diagnostics, const Diagnostics &&>::value))
  OMNI_CPP14_CONSTEXPR auto transform(F &&f) const &&
    -> with_diagnostics_t<U, Diagnostics> {
    return {
      /*value=*/this->value
        ? compat::make_optional(compat::invoke(std::forward<F>(f),
            std::move(*this->value)))
        : compat::nullopt,
      /*diagnostics=*/std::move(this->diagnostics),
    };
  }

  /** Sequences an available value through f, combining both reports in order.
   * An empty value skips f and retains its diagnostics. f returns another
   * with_diagnostics with the same diagnostics type. The default merger
   * appends issue vectors, joins text with a newline, and retains the first
   * mapping-policy snapshot. Other report types supply merge(first, second).
   * Diagnostic limits remain the mapping operation's responsibility.
   *
```
Merge :: (Diagnostics, Diagnostics) -> Diagnostics
and_then :: (T -> with_diagnostics<U, Diagnostics>, Merge = default)
  -> with_diagnostics<U, Diagnostics>
```
   */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Merge = detail::append_diagnostics_t,
    typename Result = compat::decay_t<compat::invoke_result_t<F &&, T &>>,
    typename Report = compat::invoke_result_t<Merge &&, Diagnostics &,
      Diagnostics &&>),
    (traits::is_specialization_of<omni::ryml::with_diagnostics_t, Result>()
      && std::is_constructible<Result,
        compat::invoke_result_t<F &&, T &>>::value
      && std::is_same<Diagnostics, typename Result::diagnostics_type>::value
      && std::is_constructible<Diagnostics, Diagnostics &>::value
      && std::is_constructible<Diagnostics, Report>::value))
  OMNI_CPP14_CONSTEXPR auto and_then(F &&f, Merge &&merge = {}) &
    -> Result {
    if (!this->value)
      return {compat::nullopt, this->diagnostics};

    auto next = compat::invoke(std::forward<F>(f), *this->value);
    return {
      /*value=*/std::move(next.value),
      /*diagnostics=*/compat::invoke(std::forward<Merge>(merge),
        this->diagnostics, std::move(next.diagnostics)),
    };
  }

  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Merge = detail::append_diagnostics_t,
    typename Result = compat::decay_t<compat::invoke_result_t<F &&, const T &>>,
    typename Report = compat::invoke_result_t<Merge &&, const Diagnostics &,
      Diagnostics &&>),
    (traits::is_specialization_of<omni::ryml::with_diagnostics_t, Result>()
      && std::is_constructible<Result,
        compat::invoke_result_t<F &&, const T &>>::value
      && std::is_same<Diagnostics, typename Result::diagnostics_type>::value
      && std::is_constructible<Diagnostics, const Diagnostics &>::value
      && std::is_constructible<Diagnostics, Report>::value))
  OMNI_CPP14_CONSTEXPR auto and_then(F &&f, Merge &&merge = {}) const &
    -> Result {
    if (!this->value)
      return {compat::nullopt, this->diagnostics};

    auto next = compat::invoke(std::forward<F>(f), *this->value);
    return {
      /*value=*/std::move(next.value),
      /*diagnostics=*/compat::invoke(std::forward<Merge>(merge),
        this->diagnostics, std::move(next.diagnostics)),
    };
  }

  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Merge = detail::append_diagnostics_t,
    typename Result = compat::decay_t<compat::invoke_result_t<F &&, T &&>>,
    typename Report = compat::invoke_result_t<Merge &&, Diagnostics &&,
      Diagnostics &&>),
    (traits::is_specialization_of<omni::ryml::with_diagnostics_t, Result>()
      && std::is_constructible<Result,
        compat::invoke_result_t<F &&, T &&>>::value
      && std::is_same<Diagnostics, typename Result::diagnostics_type>::value
      && std::is_constructible<Diagnostics, Diagnostics &&>::value
      && std::is_constructible<Diagnostics, Report>::value))
  OMNI_CPP14_CONSTEXPR auto and_then(F &&f, Merge &&merge = {}) &&
    -> Result {
    if (!this->value)
      return {compat::nullopt, std::move(this->diagnostics)};

    auto next = compat::invoke(std::forward<F>(f), std::move(*this->value));
    return {
      /*value=*/std::move(next.value),
      /*diagnostics=*/compat::invoke(std::forward<Merge>(merge),
        std::move(this->diagnostics), std::move(next.diagnostics)),
    };
  }

  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Merge = detail::append_diagnostics_t,
    typename Result = compat::decay_t<
      compat::invoke_result_t<F &&, const T &&>>,
    typename Report = compat::invoke_result_t<Merge &&, const Diagnostics &&,
      Diagnostics &&>),
    (traits::is_specialization_of<omni::ryml::with_diagnostics_t, Result>()
      && std::is_constructible<Result,
        compat::invoke_result_t<F &&, const T &&>>::value
      && std::is_same<Diagnostics, typename Result::diagnostics_type>::value
      && std::is_constructible<Diagnostics, const Diagnostics &&>::value
      && std::is_constructible<Diagnostics, Report>::value))
  OMNI_CPP14_CONSTEXPR auto and_then(F &&f, Merge &&merge = {}) const &&
    -> Result {
    if (!this->value)
      return {compat::nullopt, std::move(this->diagnostics)};

    auto next = compat::invoke(std::forward<F>(f), std::move(*this->value));
    return {
      /*value=*/std::move(next.value),
      /*diagnostics=*/compat::invoke(std::forward<Merge>(merge),
        std::move(this->diagnostics), std::move(next.diagnostics)),
    };
  }

  /** Recovers an empty value through f(diagnostics).
   * An available value and its diagnostics pass through. f returns the same
   * with_diagnostics type and decides which diagnostics its recovery retains.
   *
```
or_else :: (Diagnostics -> with_diagnostics<T, Diagnostics>)
  -> with_diagnostics<T, Diagnostics>
```
   */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (std::is_same<with_diagnostics_t,
      compat::decay_t<compat::invoke_result_t<F &&, Diagnostics &>>>::value
      && std::is_constructible<with_diagnostics_t,
        compat::invoke_result_t<F &&, Diagnostics &>>::value
      && std::is_constructible<compat::optional<T>,
        compat::optional<T> &>::value
      && std::is_constructible<Diagnostics, Diagnostics &>::value))
  OMNI_CPP14_CONSTEXPR auto or_else(F &&f) & -> with_diagnostics_t {
    if (this->value)
      return {this->value, this->diagnostics};

    return compat::invoke(std::forward<F>(f), this->diagnostics);
  }

  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (std::is_same<with_diagnostics_t,
      compat::decay_t<
        compat::invoke_result_t<F &&, const Diagnostics &>>>::value
      && std::is_constructible<with_diagnostics_t,
        compat::invoke_result_t<F &&, const Diagnostics &>>::value
      && std::is_constructible<compat::optional<T>,
        const compat::optional<T> &>::value
      && std::is_constructible<Diagnostics, const Diagnostics &>::value))
  OMNI_CPP14_CONSTEXPR auto or_else(F &&f) const & -> with_diagnostics_t {
    if (this->value)
      return {this->value, this->diagnostics};

    return compat::invoke(std::forward<F>(f), this->diagnostics);
  }

  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (std::is_same<with_diagnostics_t,
      compat::decay_t<compat::invoke_result_t<F &&, Diagnostics &&>>>::value
      && std::is_constructible<with_diagnostics_t,
        compat::invoke_result_t<F &&, Diagnostics &&>>::value
      && std::is_constructible<compat::optional<T>,
        compat::optional<T> &&>::value
      && std::is_constructible<Diagnostics, Diagnostics &&>::value))
  OMNI_CPP14_CONSTEXPR auto or_else(F &&f) && -> with_diagnostics_t {
    if (this->value)
      return {std::move(this->value), std::move(this->diagnostics)};

    return compat::invoke(std::forward<F>(f), std::move(this->diagnostics));
  }

  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (std::is_same<with_diagnostics_t,
      compat::decay_t<
        compat::invoke_result_t<F &&, const Diagnostics &&>>>::value
      && std::is_constructible<with_diagnostics_t,
        compat::invoke_result_t<F &&, const Diagnostics &&>>::value
      && std::is_constructible<compat::optional<T>,
        const compat::optional<T> &&>::value
      && std::is_constructible<Diagnostics, const Diagnostics &&>::value))
  OMNI_CPP14_CONSTEXPR auto or_else(F &&f) const && -> with_diagnostics_t {
    if (this->value)
      return {std::move(this->value), std::move(this->diagnostics)};

    return compat::invoke(std::forward<F>(f), std::move(this->diagnostics));
  }

  /** Transforms diagnostics with f while retaining value availability.
   * Reports are transformed on success as well as failure, so warnings are
   * included. Rvalue overloads move the retained value.
   *
```
transform_diagnostics :: (Diagnostics -> Report) -> with_diagnostics<T, Report>
```
   */
  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Report = compat::decay_t<
      compat::invoke_result_t<F &&, Diagnostics &>>),
    (std::is_constructible<Report,
      compat::invoke_result_t<F &&, Diagnostics &>>::value
      && std::is_constructible<compat::optional<T>,
        compat::optional<T> &>::value))
  OMNI_CPP14_CONSTEXPR auto transform_diagnostics(F &&f) &
    -> with_diagnostics_t<T, Report> {
    return {
      /*value=*/this->value,
      /*diagnostics=*/compat::invoke(std::forward<F>(f), this->diagnostics),
    };
  }

  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Report = compat::decay_t<
      compat::invoke_result_t<F &&, const Diagnostics &>>),
    (std::is_constructible<Report,
      compat::invoke_result_t<F &&, const Diagnostics &>>::value
      && std::is_constructible<compat::optional<T>,
        const compat::optional<T> &>::value))
  OMNI_CPP14_CONSTEXPR auto transform_diagnostics(F &&f) const &
    -> with_diagnostics_t<T, Report> {
    return {
      /*value=*/this->value,
      /*diagnostics=*/compat::invoke(std::forward<F>(f), this->diagnostics),
    };
  }

  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Report = compat::decay_t<
      compat::invoke_result_t<F &&, Diagnostics &&>>),
    (std::is_constructible<Report,
      compat::invoke_result_t<F &&, Diagnostics &&>>::value
      && std::is_constructible<compat::optional<T>,
        compat::optional<T> &&>::value))
  OMNI_CPP14_CONSTEXPR auto transform_diagnostics(F &&f) &&
    -> with_diagnostics_t<T, Report> {
    return {
      /*value=*/std::move(this->value),
      /*diagnostics=*/compat::invoke(std::forward<F>(f),
        std::move(this->diagnostics)),
    };
  }

  OMNI_CONSTRAINED_TEMPLATE((typename F,
    typename Report = compat::decay_t<
      compat::invoke_result_t<F &&, const Diagnostics &&>>),
    (std::is_constructible<Report,
      compat::invoke_result_t<F &&, const Diagnostics &&>>::value
      && std::is_constructible<compat::optional<T>,
        const compat::optional<T> &&>::value))
  OMNI_CPP14_CONSTEXPR auto transform_diagnostics(F &&f) const &&
    -> with_diagnostics_t<T, Report> {
    return {
      /*value=*/std::move(this->value),
      /*diagnostics=*/compat::invoke(std::forward<F>(f),
        std::move(this->diagnostics)),
    };
  }

  // TODO: [medium] Consider constructor conveniences; retain aggregate
  // initialization for the existing value/diagnostics interface.
};

} // namespace ryml
} // namespace omni
