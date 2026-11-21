#pragma once

/** Shared mapping configuration, diagnostics, and read/write states. */

#include <omnirefl/traits.hpp>
#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/static_string.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <type_traits>
#include <string>
#include <utility>
#include <vector>

namespace omni {

template <typename T>
struct type_t;

// Only backend-independent JSON/YAML mapping contracts belong here.
namespace json {

/** Member-name mapping contract.
 * Each mapped member has a canonical name and may accept aliases. The sets
 * consisting of each member's canonical name and aliases must be disjoint
 * across members. The caller must ensure this; collisions are not validated,
 * and mapping results are unspecified when this precondition is violated.
 *
 * If any canonical occurrence exists, its aliases are ignored regardless of
 * value validity. Otherwise aliases identify the member. Duplicate policies
 * select among canonical occurrences or, only when absent, among aliases.
 * Only the selected occurrence is decoded; invalid canonical input never
 * falls back to an alias. Present but invalid input is not a missing member.
 */

/** JSON/YAML parsing and mapping failures, independent of the parser backend.
 * Field presence and diagnostic limits follow the requested mapping contract.
 */
enum class issue_code : std::uint8_t {
  unexpected_kind,
  invalid_scalar,
  out_of_range, //< Value lies outside the range accepted by the mapping.
  missing_field,
  unknown_field,
  duplicate_field,
  parse_error,
  warning_limit_exceeded,
  duplicate_element //< Repeated value in a unique-element destination.
};

/** Value categories used in mapping expectations and observed input.
 * These describe values, not destination-language storage types. */
enum class value_kind : std::uint8_t {
  unknown, //< No category is known or applicable.
  scalar, //< A non-collection value whose category has not been resolved.
  mapping,
  sequence,
  string,
  boolean,
  integer, //< An integral number.
  unsigned_integer, //< A non-negative integral number.
  number,
  null
};

/** Controls repeated object names and elements of unique destinations.
 * Last implies allow; warn is ignored when duplicates are rejected. */
enum duplicate_policy : std::uint8_t {
  reject = 0,
  allow = 1 << 0, //< Keep the first occurrence, without a warning.
  last = 1 << 1, //< Keep the last occurrence.
  warn = 1 << 2 //< Warn about repeated field names when duplicates are allowed.
};

OMNI_ENABLE_BIT_FLAGS(duplicate_policy)

/** Input fields absent from the struct; warn is ignored without allow. */
enum class unknown_policy : std::uint8_t {
  reject = 0,
  allow = 1 << 0, //< Ignore unknown fields without a warning.
  warn = 1 << 1 //< Warn about unknown fields only when allow is set.
};

OMNI_ENABLE_BIT_FLAGS(unknown_policy)

/**
 * Controls mapping errors independently of the JSON or YAML parsing algorithm.
 *
 * @details
 * Mapping returns with_diagnostics_t: a valid
 * value may accompany warnings, whereas expected<T, Diagnostics> holds either
 * the value or diagnostics. Error limits allow collecting more diagnostics;
 * they do not permit returning an invalid value.
 *
 * A partially initialized object is an implementation detail, not a result.
 * Exposing it would constrain implementations that construct the result in
 * one expression from mapped fields. Field types can represent recoverable
 * invalid input explicitly. A future invalid-value recovery mechanism may
 * define what it exposes during mapping without promising a partial result.
 */
struct error_policy {
  // Number of mapping errors processing may pass before stopping on the next.
  // Warnings do not count. Zero stops at the first error.
  std::uint16_t error_limit = 0;

  // Maximum permitted warnings; unset means unlimited. The next warning
  // exceeds the limit, produces an error, and prevents returning a value.
  // Zero fails on the first warning.
  compat::optional<std::uint16_t> warning_limit{};

  // Reject duplicates unless allow or last is set; warn alone does not allow.
  duplicate_policy duplicates = duplicate_policy::reject;

  // Reject input fields absent from the struct by default.
  unknown_policy unknown_fields = unknown_policy::reject;
};

} // namespace json

namespace ryml {

struct diagnostics;

/** Render issue records using their source tree while it is alive. */
struct render_diagnostics_t {
  std::string operator()(const ::ryml::Tree &tree, const diagnostics &d) const;

};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr render_diagnostics_t render_diagnostics{};
#else
static constexpr const render_diagnostics_t &render_diagnostics =
  compat::detail::shared_state<render_diagnostics_t>::value;
#endif

/** Deserialization configuration.
 * In C++20, aggregate deduction supports config_t{.transform_diagnostics = f}.
 * Earlier standards can use tdm_transform_diagnostics to change the callable
 * type without spelling the resulting config specialization. */
template <typename TransformDiagnostics = render_diagnostics_t>
struct config_t {
  json::error_policy error_policy OMNI_CPP14_DEFAULT_INIT();

  // Called with the live source tree and collected diagnostics on every result.
  TransformDiagnostics transform_diagnostics OMNI_CPP14_DEFAULT_INIT();

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
      this->error_policy,
      /*transform_diagnostics=*/compat::invoke(
        std::forward<F>(f), this->transform_diagnostics),
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
      std::move(this->error_policy),
      /*transform_diagnostics=*/compat::invoke(std::forward<F>(f),
        std::move(this->transform_diagnostics)),
    };
  }
};

/** Whether T is a config_t specialization, ignoring cv/ref qualifiers. */
template <typename T>
struct is_config:
    compat::bool_constant<traits::is_specialization_of<config_t, T>()> {};

// C++14 adds variable templates for trait values.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <typename T>
constexpr bool is_config_v = is_config<T>::value;
#endif

// C++20 callers can constrain configuration arguments directly.
#if defined(__cpp_concepts) && 201907L <= __cpp_concepts
template <typename T>
concept config_like = is_config<T>::value;
#endif

/**
 * Describes an input issue without rendering a message or deciding failure.
 */
struct issue {
  struct path_segment {
    // Field name borrowed from metadata or the caller-owned tree.
    c4::csubstr field;

    // Sequence position; empty for a named field.
    compat::optional<std::size_t> index;
  };

  // Group byte-sized kinds and the warning flag before rendering data.
  // Validation or conversion that failed.
  json::issue_code reason;

  // Input kind required by the destination field.
  json::value_kind expected;

  // Input kind encountered at the failing node.
  json::value_kind actual;

  // True for accepted input conditions reported by the selected policy.
  bool warning = false;

  // Field names and sequence indices from the root to the issue.
  std::vector<path_segment> path;

  // node_id refers to the caller-owned tree; field views may borrow its keys.
  // Missing fields may have no corresponding node.
  ::ryml::id_type node_id = ::ryml::NONE;

  // TODO: Revisit path storage if C++11 constant evaluation becomes necessary.
  // TODO: Accept retained parser source-location metadata when mapping;
  // standalone trees do not retain source positions.
  // TODO: Add constraint metadata when needed.
};

/** Collected mapping issues without ownership of their source tree.
 * Keep the tree and borrowed character storage alive and unchanged while
 * rendering diagnostics. Field paths may borrow metadata or source keys. */
struct diagnostics {
  // Mapping-policy snapshot; the deserialize transformation is not retained.
  omni::ryml::config_t<> config;

  // Validation and conversion issues in traversal order, including warnings.
  std::vector<issue> issues;

  // Owns RapidYAML syntax-error text returned by core::parse.
  // Empty when mapping an already parsed tree.
  std::string parse_error_message;
};

/** Diagnostics result produced by Config, preserving its cv/ref qualifiers. */
template <typename Config>
using diagnostics_result_t = compat::decay_t<compat::invoke_result_t<
  decltype((std::declval<Config>().transform_diagnostics)),
  const ::ryml::Tree &,
  diagnostics>>;

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
and_then :: (T -> with_diagnostics<U, Diagnostics>)
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
};

namespace detail {

// The C++11 result is defined after the built-in mapping types are complete.
template <typename T>
struct to_type_cpp11_trailing_return;

template <typename T>
struct from_type_cpp11_trailing_return;

} // namespace detail

namespace reflected_scope {

/** Callable form of the node-to-type customization lookup.
 *
```
mapping_to_type :: (state, type<T>) -> Mapping
```
 * State is accepted for compatibility with local mapping selectors.
 * The default selection depends only on T. */
struct mapping_to_type_t {
  template <typename State, typename T>
  OMNI_CPP14_CONSTEXPR auto operator()(const State &, omni::type_t<T>) const
    OMNI_CPP11_TRAILING_RETURN(
      typename omni::ryml::detail::to_type_cpp11_trailing_return<T>::type);
};

/** Callable form of the type-to-node customization lookup.
 *
```
mapping_from_type :: (state, type<T>) -> Mapping
```
 * The default selection depends only on T. */
struct mapping_from_type_t {
  template <typename State, typename T>
  OMNI_CPP14_CONSTEXPR auto operator()(const State &, omni::type_t<T>) const
    OMNI_CPP11_TRAILING_RETURN(
      typename omni::ryml::detail::from_type_cpp11_trailing_return<T>::type);
};

} // namespace reflected_scope

namespace detail {

// Default states can be constructed without including type_mapping.hpp.
#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr reflected_scope::mapping_to_type_t default_map_node{};
inline constexpr reflected_scope::mapping_from_type_t default_write_node{};
#else
static constexpr const reflected_scope::mapping_to_type_t &default_map_node =
  compat::detail::shared_state<reflected_scope::mapping_to_type_t>::value;
static constexpr const reflected_scope::mapping_from_type_t
  &default_write_node =
    compat::detail::shared_state<reflected_scope::mapping_from_type_t>::value;
#endif

// String providers keep the path independent of runtime traversal state.
struct root_path_name {
  static constexpr const char *str() { return "root"; }
};
struct path_dot {
  static constexpr const char *str() { return "."; }
};
struct path_element {
  static constexpr const char *str() { return "[]"; }
};

template <typename Field>
struct name_as_string {
  static constexpr const char *str() { return Field::name(); }
};

template <typename Name>
struct field_path_name: cpp11_static_string<path_dot, Name> {};

template <typename... Segments>
struct schema_path;

// Re-entering the same reflected field revisits a recursive schema edge.
// Reuse its first schema path rather than instantiate unbounded state types.
template <typename Prefix, typename Segment, typename... Remaining>
struct schema_field_path;

template <typename... Prefix, typename Segment>
struct schema_field_path<schema_path<Prefix...>, Segment> {
  using type = schema_path<Prefix..., Segment>;
};

template <typename... Prefix, typename Segment, typename Head, typename... Tail>
struct schema_field_path<schema_path<Prefix...>, Segment, Head, Tail...>:
    compat::conditional_t<std::is_same<Segment, Head>::value,
      compat::type_identity<schema_path<Prefix..., Segment>>,
      schema_field_path<schema_path<Prefix..., Head>, Segment, Tail...>> {};

template <typename... Segments>
struct schema_path {
  template <typename Name>
  using child = schema_path<Segments..., field_path_name<Name>>;

  template <typename Field>
  using field = typename schema_field_path<schema_path<>,
    field_path_name<name_as_string<Field>>, Segments...>::type;

  using element = schema_path<Segments..., path_element>;
  using text = cpp11_static_string<Segments...>;
#if 202002L <= OMNI_CPLUSPLUS
  using string_type = nttp_string<omni::detail::static_string_literal<
    text::size() + 1>{text::view()}>;
#else
  using string_type = text;
#endif
};

} // namespace detail

/** Deserialization configuration and traversal progress passed to readers.
 * Config and the mapping selector are borrowed. Copies do not copy their
 * contents or allocate storage.
 * Counters include issues reported by preceding operations in the enclosing
 * traversal. The path describes the C++ schema, independently of input keys.
 * Mapping callbacks take a generic State, constrained by is_read_state,
 * because child paths and local selectors produce different state types.
 * Custom mappings retain it by passing state onward; custom field traversal
 * extends it with child(OMNI_META_STRING("field")) and element() for containers.
 *
```cpp
// Inside a generic mapping selector, from C++17:
if constexpr (state.path().ends_with("root.work_address")) {
  // Select the work-address mapping; other address fields keep their defaults.
}
```
 */
template <typename Config = omni::ryml::config_t<>,
  typename MapNode = reflected_scope::mapping_to_type_t,
  typename Path = detail::schema_path<detail::root_path_name>>
struct read_state_t {
  const Config &config;
  std::size_t error_count OMNI_CPP14_DEFAULT_INIT();
  std::size_t warning_count OMNI_CPP14_DEFAULT_INIT();

  // (state, type<T>) -> mapping callable; borrowed throughout traversal.
  const MapNode &map_node OMNI_CPP14_DEFAULT_INIT(
    detail::default_map_node);

#if 201402L > OMNI_CPLUSPLUS
  // C++11 aggregates cannot provide the default for this reference member.
  constexpr read_state_t(const Config &options, std::size_t errors = 0,
    std::size_t warnings = 0, const MapNode &mapping =
      detail::default_map_node)
    : config(options), error_count(errors), warning_count(warnings),
      map_node(mapping) {}
#endif

  /** Canonical schema path: root.field, with [] for container values.
   * Independent of runtime keys, indices, and input aliases. Recursive schema
   * edges reuse their first path; this is not a runtime ancestry trace. */
  static constexpr auto path() -> typename Path::string_type { return {}; }

  /** Child state for custom traversal into a named field.
   * Appends text; field(meta) additionally recognizes recursive schema edges. */
  template <typename Name>
  constexpr auto child(Name) const
    -> read_state_t<Config, MapNode, typename Path::template child<Name>> {
    return {this->config, this->error_count, this->warning_count,
      this->map_node};
  }

  /** Child state for a reflected field, retaining recursive schema identity. */
  template <typename Field>
  constexpr auto field(Field) const
    -> read_state_t<Config, MapNode, typename Path::template field<Field>> {
    return {this->config, this->error_count, this->warning_count,
      this->map_node};
  }

  /** Container-value state; runtime positions and keys share the [] marker. */
  constexpr auto element() const
    -> read_state_t<Config, MapNode, typename Path::element> {
    return {this->config, this->error_count, this->warning_count,
      this->map_node};
  }

  /** Whether an error or warning budget has been exceeded. */
  constexpr bool limit_exceeded() const {
    return this->config.error_policy.error_limit < this->error_count
      || (this->config.error_policy.warning_limit
        && *this->config.error_policy.warning_limit < this->warning_count);
  }
};

/** Serialization policy, local mapping selection, and schema position.
 * BorrowStrings permits borrowing source-backed strings; temporary owning
 * conversions are still copied. The mapping selector is borrowed. Mapping
 * callbacks take a generic State constrained by is_write_state, preserving
 * the child path and local selector when forwarding it. */
template <bool BorrowStrings,
  typename MapNode = reflected_scope::mapping_from_type_t,
  typename Path = detail::schema_path<detail::root_path_name>>
struct write_state_t {
  static constexpr bool borrow_strings = BorrowStrings;
  const MapNode &map_node OMNI_CPP14_DEFAULT_INIT(detail::default_write_node);

#if 201402L > OMNI_CPLUSPLUS
  // C++11 aggregates cannot default-initialize a reference member.
  constexpr write_state_t(const MapNode &mapping = detail::default_write_node)
    : map_node(mapping) {}
#endif

  /** Canonical schema path; see read_state_t::path. */
  static constexpr auto path() -> typename Path::string_type { return {}; }

  /** Child state for custom traversal into a named field.
   * Appends text; field(meta) additionally recognizes recursive schema edges. */
  template <typename Name>
  constexpr auto child(Name) const
    -> write_state_t<BorrowStrings, MapNode,
      typename Path::template child<Name>> {
    return {this->map_node};
  }

  /** Child state retaining a reflected field's recursive schema identity. */
  template <typename Field>
  constexpr auto field(Field) const
    -> write_state_t<BorrowStrings, MapNode,
      typename Path::template field<Field>> {
    return {this->map_node};
  }

  /** Container-value state; runtime positions and keys share the [] marker. */
  constexpr auto element() const
    -> write_state_t<BorrowStrings, MapNode, typename Path::element> {
    return {this->map_node};
  }
};

namespace detail {

template <typename T>
struct is_write_state: std::false_type {};

template <bool BorrowStrings, typename MapNode, typename Path>
struct is_write_state<write_state_t<BorrowStrings, MapNode, Path>>:
    std::true_type {};

} // namespace detail

/** Whether T is a read_state_t specialization, ignoring cv/ref qualifiers. */
template <typename T>
struct is_read_state:
    compat::bool_constant<traits::is_specialization_of<read_state_t, T>()> {};

/** Whether T is a write_state_t specialization, ignoring cv/ref qualifiers. */
template <typename T>
struct is_write_state:
    detail::is_write_state<compat::remove_cvref_t<T>> {};

// C++14 adds variable templates for trait values.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <typename T>
constexpr bool is_read_state_v = is_read_state<T>::value;

template <typename T>
constexpr bool is_write_state_v = is_write_state<T>::value;
#endif

// C++20 concepts constrain customization state without exposing its parameters.
#if defined(__cpp_concepts) && 201907L <= __cpp_concepts
template <typename T>
concept read_state_like = is_read_state<T>::value;

template <typename T>
concept write_state_like = is_write_state<T>::value;
#endif

// TODO: [high] Consider a std::size_t Depth template parameter for traversal
// state.

} // namespace ryml
} // namespace omni

// TODO: [high] Expose enclosing_record as a type alias on traversal state,
// keeping record type information separate from the schema path string.

// TODO: [medium] Reduce state instantiations for records with several recursive
// fields: edge canonicalization retains each permutation of distinct fields
// before a repeated edge reuses its path.
