#pragma once

/** Shared mapping configuration, diagnostics, and read/write states. */

#include <omnirefl/traits.hpp>
#include <omnirefl/extensions/compat.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace omni {

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
  compat::optional<std::uint16_t> warning_limit = compat::nullopt;

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

constexpr render_diagnostics_t render_diagnostics{};

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

/**
 * Holds the deserialized value, if available, and diagnostics even on failure.
 * Diagnostics can hold issue records or a rendered message.
 */
template <typename T,
  typename Diagnostics = omni::ryml::diagnostics>
struct with_diagnostics_t {
  // Available only when mapping succeeds.
  compat::optional<T> value;

  // Retained issue records, or the result of transforming them for display.
  Diagnostics diagnostics;
};

/** Borrowed path through active mapping states.
 * Copies retain parent links without allocating. Referenced parent paths must
 * remain alive and unchanged until child mappings finish. Issues materialize
 * their own segment arrays, retaining the field-name storage they borrow. */
struct mapping_path {
  // Root paths have depth zero; their parent and segment fields are unused.
  const mapping_path *parent;
  c4::csubstr field;
  // Keep active paths trivial even with older non-trivial optional libraries.
  std::size_t index;
  bool has_index;
  std::size_t depth;

  /** Copy the path's segments into root-to-leaf order for diagnostics. */
  std::vector<issue::path_segment> materialize() const;
};

/** Deserialization configuration and traversal progress passed to readers.
 * Config is borrowed; path links refer to active ancestor states. Copies do
 * not copy configuration or allocate path storage. Counters include issues
 * reported by preceding operations in the enclosing traversal. */
template <typename Config = omni::ryml::config_t<>>
struct read_state_t {
  const Config &config;
  mapping_path path;
  std::size_t error_count;
  std::size_t warning_count;

  /** Whether an error or warning budget has been exceeded. */
  constexpr bool limit_exceeded() const {
    return this->config.error_policy.error_limit < this->error_count
      || (this->config.error_policy.warning_limit
        && *this->config.error_policy.warning_limit < this->warning_count);
  }

  /** Descend one path segment, preserving configuration and issue counts.
   * This state must remain alive while the returned child is used. */
  OMNI_CPP14_CONSTEXPR read_state_t child(issue::path_segment segment)
    const & {
    return {
      /*config=*/this->config,
      /*path=*/{
        /*parent=*/&this->path,
        /*field=*/segment.field,
        /*index=*/segment.index ? *segment.index : 0,
        /*has_index=*/bool(segment.index),
        /*depth=*/this->path.depth + 1,
      },
      /*error_count=*/this->error_count,
      /*warning_count=*/this->warning_count,
    };
  }

  // Child paths borrow the parent, so reject a temporary parent state.
  read_state_t child(issue::path_segment) const && = delete;
};

/** Serialization string-storage policy passed to writers.
 * BorrowStrings permits borrowing source-backed strings; temporary owning
 * string conversions are still copied. This state has no runtime data. */
template <bool BorrowStrings>
struct write_state_t {
  static constexpr bool borrow_strings = BorrowStrings;
};

/** Whether T is a read_state_t specialization, ignoring cv/ref qualifiers. */
template <typename T>
struct is_read_state:
    compat::bool_constant<traits::is_specialization_of<read_state_t, T>()> {};

/** Whether T is a write_state_t specialization, ignoring cv/ref qualifiers. */
template <typename T>
struct is_write_state:
    compat::bool_constant<traits::is_specialization_of<write_state_t, T>()> {};

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

} // namespace ryml
} // namespace omni

#include <omnirefl/serialization/ryml/impl/state.hpp>
