#pragma once

#if !defined(OMNI_TOOL_RUN) \
  && !defined(OMNI_INCLUDED_GENERATED_REFLECTION_HEADER)
#  error \
    "Include the generated reflection header before this serialization header. " \
    "With CMake, call omni_reflected_target(<target>)."
#endif

/**
 * YAML and JSON serialization backed by rapidyaml and omnirefl metadata.
 * Deserialization composes parsing, tree validation, and reflected field
 * assignment with configurable recovery and diagnostics rendered on demand.
 * Serialization writes reflected records, supported scalars, and vectors with
 * as_yaml or as_json.
 * Compatible with C++11 and later.
 *
 * Examples prefer C++20 syntax. Compatibility interfaces are available for
 * C++11; see extras/extensions/serialization/tests/test.cpp for complete usage.
 *
 * Given a reflectable person type and a std::string source:
 *
```cpp
auto result = omni::ryml::deserialize(omni::type<person>, source) //
  .transform_error(omni::ryml::render_diagnostics);

if (result.value) {
  const auto json = omni::ryml::as_json(*result.value);
  const auto yaml = omni::ryml::as_yaml(*result.value);
}
// result.diagnostics contains the rendered diagnostics.
```
 */

#include <omnirefl/reflected_scope.hpp>

#include <omnirefl/extensions/compat.hpp>

#include <ryml.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
// FIXME: [low] Freestanding: public diagnostics and text APIs require hosted
// owning strings and vectors; provide an alternative representation.
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace omni {
// Only backend-independent JSON/YAML mapping contracts belong here.
// TODO: Move to a shared header if another JSON parser is adopted.
namespace json {

/// JSON/YAML parsing and mapping failures, independent of the parser backend.
/// Field presence and diagnostic limits refer to the requested mapping contract.
enum class issue_code : std::uint8_t {
  unexpected_kind,
  invalid_scalar,
  out_of_range, // Value lies outside the range accepted by the mapping.
  missing_field,
  unknown_field,
  duplicate_field,
  parse_error,
  warning_limit_exceeded
};

/// Value categories used in mapping expectations and observed input.
/// These describe values, not destination-language storage types.
enum class value_kind : std::uint8_t {
  unknown, // No category is known or applicable.
  scalar, // A non-collection value whose category has not been resolved.
  mapping,
  sequence,
  string,
  boolean,
  integer, // An integral number.
  unsigned_integer, // A non-negative integral number.
  number,
  null
};

/// Controls duplicate fields without merging occurrences.
/// Last implies allow; warn is ignored when duplicates are rejected.
enum duplicate_policy : std::uint8_t {
  reject = 0,
  allow = 1 << 0, // Keep the first occurrence, without a warning.
  last = 1 << 1, // Keep the last occurrence.
  warn = 1 << 2 // Warn about repeated field names when duplicates are allowed.
};

OMNI_ENABLE_BIT_FLAGS(duplicate_policy)

/// Controls input fields absent from the struct. Warn is ignored without allow.
enum class unknown_policy : std::uint8_t {
  reject = 0,
  allow = 1 << 0, // Ignore unknown fields without a warning.
  warn = 1 << 1 // Warn about unknown fields only when allow is set.
};

OMNI_ENABLE_BIT_FLAGS(unknown_policy)

/**
 * Controls mapping errors independently of the JSON or YAML parsing algorithm.
 *
 * @details
 * Mapping returns with_diagnostics: a valid
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

  // TODO: Consider warning thresholds per policy.
};

/**
 * Select recovery methods for missing fields where applicable.
 * Invalid values and explicit null are not recovered by these flags.
 *
 * Flags may be combined. An exposed field default takes precedence over
 * default construction when both methods are enabled.
 * Successful recovery does not itself produce a warning.
 */
enum class recovery_policy : std::uint8_t {
  // Use field defaults where Omnirefl can expose them.
  field_defaults = 1 << 0,
  // Use value initialization (T{}) where supported, without a reflected
  // default.
  default_initializable = 1 << 1,

  // TODO: Define when default initialization is valid recovery for the
  // containing type; e.g. recovered map keys may collide and lose entries.
  // TODO: Consider reporting defaulted fields separately from warnings.
};

OMNI_ENABLE_BIT_FLAGS(recovery_policy)

} // namespace json

namespace ryml {

/// Configures mapping diagnostics and recovery of missing fields.
struct settings {
  json::error_policy error_policy;

  // No missing-field recovery unless explicitly configured.
  compat::optional<json::recovery_policy> recovery = compat::nullopt;

  // TODO: [high] Add a configurable input-size limit to settings and reject
  // oversized source text before parsing or allocating a tree. A std::string
  // argument is already buffered; callers must also bound request-body reads
  // to prevent oversized REST requests from consuming memory before this call.

  // TODO: Make settings select SAX-style validation and field assignment as
  // rapidyaml emits events, or the current parse-whole-tree -> map pipeline.
  // Event-based mapping could reject invalid input before parsing and retaining
  // a huge remaining subtree. Preserve error and recovery policies, and
  // diagnostics; check missing fields when their object closes. Resolve YAML
  // aliases and diagnostic-view ownership before enabling the SAX path.

  // TODO: [high] Detect at compile time whether a model permits unbounded
  // nesting (e.g. a vector of itself); consider a traversal-depth limit in
  // settings to prevent stack exhaustion from deeply nested or malicious input.

  // TODO: [high] Define a union deserialization policy before mapping
  // std::variant. JSON Schema commonly represents alternatives with oneOf;
  // trying every alternative is ambiguous and may repeat expensive work.
  // Prefer a configurable discriminator that selects an alternative from a
  // field value. Decide how metadata names alternatives, where the
  // discriminator lives, and whether untagged matching is ever allowed.

  // TODO: Consider storing ryml::ParserOptions if rapidyaml's defaults prove
  // insufficient. Direct storage would make settings non-constexpr; do not
  // mirror ParserOptions' private flags in another type.
};

// TODO: Consider global-default customization without requiring users to name
// settings before this header has declared it; settings has no separate header.

/**
 * Describes an input issue without rendering a message or deciding failure.
 */
struct issue {
  struct path_segment {
    // Field name borrowed from metadata or the retained tree.
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

  // Borrow the decoded scalar from the retained tree; render only on request.
  c4::csubstr value;

  // Field names and sequence indices from the root to the issue.
  // TODO: Revisit path storage if C++11 constant evaluation becomes necessary.
  std::vector<path_segment> path;

  // Node and field views borrow the tree retained by diagnostics.
  // Missing fields may have no corresponding node.
  ::ryml::id_type node_id = ::ryml::NONE;

  // Source positions are unavailable from a standalone ryml::Tree.
  // TODO: Accept retained source-location metadata when mapping.
  std::size_t offset = std::string::npos;
  std::size_t line = std::string::npos;
  std::size_t column = std::string::npos;

  // TODO: Add constraint metadata and renderer customization when needed.
};

/**
 * Keeps issue records and an owning tree or borrowed node view for rendering.
 * Issue node IDs and input string views refer into that tree.
 *
 * When Owning is false, keep the caller's tree alive and unchanged while using
 * diagnostics. Moving or changing it can invalidate node and string views.
 * When true, move this object; copying does not rebind issue views to its copy.
 * The copy constructor is left implicit to keep initialization simple and avoid
 * explicit copy/move special-member declarations.
 */
template <bool Owning>
struct diagnostics {
  // Snapshot for inspection; mapping receives its settings separately.
  omni::ryml::settings settings;

  // Retain the tree or refer to the caller's subtree, depending on Owning.
  // External string storage referenced by a caller-supplied tree must remain
  // alive and unchanged while using diagnostics, even when Owning is true.
  compat::conditional_t<Owning, ::ryml::Tree, ::ryml::ConstNodeRef> tree;

  // Validation and conversion issues in traversal order, including warnings.
  std::vector<issue> issues;

  // Owns the parser's text for a parse_error issue; mapping leaves it empty.
  std::string parse_message;

  // True when an error or warning limit requires stopping.
  bool stopped() const {
    return settings.error_policy.error_limit < static_cast<std::size_t>( //
             std::count_if(
               issues.begin(),
               issues.end(),
               [](const issue &e) { return !e.warning; }))
      || std::any_of(
        issues.begin(),
        issues.end(),
        [](const issue &e) {
          return json::issue_code::warning_limit_exceeded == e.reason;
        });
  }
};

/**
 * Holds the deserialized value, if available, and diagnostics even on failure.
 * Diagnostics can hold issue records or a rendered message.
 */
template <typename T,
  typename Diagnostics = omni::ryml::diagnostics</*owning=*/true>>
struct with_diagnostics {
  // Available only when mapping succeeds, including permitted recovery.
  compat::optional<T> value;

  // Retained issue records, or the result of transforming them for display.
  Diagnostics diagnostics;

  /**
   * Consume this result, applying f to an available value and keeping
   * diagnostics. When no value is available, do not call f.
   * Consumes an rvalue result; f must return a value, not void.
   */
  template <typename F>
  auto map_value(F &&f)
    && -> with_diagnostics<compat::decay_t<compat::invoke_result_t<F &&, T &&>>,
      Diagnostics> {
    return {
      /*value=*/value ? compat::make_optional(
                          compat::invoke(std::forward<F>(f), std::move(*value)))
                      : compat::nullopt,
      /*diagnostics=*/std::move(diagnostics),
    };
  }

  /**
   * Consume this result, keeping the optional value and applying f to
   * diagnostics. Calls f whether or not a value is available, preserving
   * the value. The returned diagnostics have the type returned by f.
   */
  template <typename F>
  auto map_diagnostics(F &&f) && -> with_diagnostics<T,
    compat::decay_t<compat::invoke_result_t<F &&, Diagnostics &&>>> {
    return {
      /*value=*/std::move(value),
      /*diagnostics=*/
      compat::invoke(std::forward<F>(f), std::move(diagnostics)),
    };
  }

  /**
   * Alias for map_diagnostics: transform diagnostics even alongside a value.
   */
  template <typename F>
  auto map_error(F &&f) && -> decltype(std::move(*this).map_diagnostics(
    std::forward<F>(f))) {
    return std::move(*this) //
      .map_diagnostics(std::forward<F>(f));
  }

  /**
   * Alias for map_value, matching expected's value transformation interface.
   */
  template <typename F>
  auto transform(
    F &&f) && -> decltype(std::move(*this).map_value(std::forward<F>(f))) {
    return std::move(*this) //
      .map_value(std::forward<F>(f));
  }

  /**
   * Alias for map_error, keeping any value while transforming diagnostics.
   * Unlike expected, calls f even when a value is available.
   *
   * Retains warnings alongside a valid value and errors when no value exists.
   */
  template <typename F>
  auto transform_error(
    F &&f) && -> decltype(std::move(*this).map_error(std::forward<F>(f))) {
    return std::move(*this) //
      .map_error(std::forward<F>(f));
  }

  // TODO: Collect defaulted fields separately from warnings and errors.
  // TODO: Add and_then for generic expected-style sequencing; specify how to
  // retain diagnostics from both the input and the returned result.
  // TODO: Add or_else for generic expected-style recovery; specify how to
  // retain diagnostics when a value and issues are both present.
};

/**
 * Parse text into an owning tree, returning syntax errors without throwing.
 *
 * ParserOptions are passed directly to ryml. Location tracking, if enabled,
 * ends with this call; the returned tree does not retain the parser.
 *
```cpp
// Supply a reflected person and an owning std::string source.
auto result = parse(std::move(source)) //
  .transform([](::ryml::Tree tree) {
    return map_tree(omni::type<person>, std::move(tree)) //
      .transform_error(render_diagnostics);
  });
// Syntax errors use expected; mapping retains optional person and diagnostics.
```
 */
inline compat::expected<::ryml::Tree, std::string> parse(std::string source,
  ::ryml::ParserOptions options = {});

namespace core {

// Unbound mapping operations receive all settings explicitly.
template <typename Record>
with_diagnostics<Record, diagnostics<true>> map_tree(const settings &options,
  omni::type_t<Record> record_type,
  ::ryml::Tree tree);

template <typename Record>
with_diagnostics<Record, diagnostics<false>> map_tree(const settings &options,
  omni::type_t<Record> record_type,
  ::ryml::ConstNodeRef node);

template <typename Record>
with_diagnostics<Record, diagnostics<true>> deserialize(const settings &options,
  omni::type_t<Record> record_type,
  std::string source);

} // namespace core

/// Maps an owning tree or borrowed subtree with the bound settings.
struct map_tree_t {
  omni::ryml::settings settings;

  /// Returns a new binding with transformed settings; preserves this binding.
  template <typename F>
  OMNI_CPP14_CONSTEXPR auto transform_settings(F &&f) const ->
    typename std::enable_if<
      compat::is_invocable<F &&, omni::ryml::settings>::value
        && std::is_same<omni::ryml::settings,
          compat::invoke_result_t<F &&, omni::ryml::settings>>::value,
      map_tree_t>::type {
    return {compat::invoke(std::forward<F>(f), omni::ryml::settings{settings})};
  }

  // Own the tree; move it after mapping so diagnostic views remain valid.
  template <typename Record>
  with_diagnostics<Record, diagnostics<true>>
    operator()(omni::type_t<Record> record_type, ::ryml::Tree tree) const {
    return core::map_tree(settings, record_type, std::move(tree));
  }

  // Keep the caller's tree alive and unchanged while using borrowed
  // diagnostics.
  template <typename Record>
  with_diagnostics<Record, diagnostics<false>> operator()(
    omni::type_t<Record> record_type,
    ::ryml::ConstNodeRef node) const {
    return core::map_tree(settings, record_type, node);
  }
};

/**
 * Parses and maps input, returning a valid value with warnings or errors alone.
 *
```cpp
const auto load = deserialize.transform_settings([limit](ryml::settings s) {
  s.error_policy.error_limit = limit;
  s.error_policy.unknown_fields = json::unknown_policy::allow;
  return s;
});
auto result = load(omni::type_t<person>{}, source);
```
 */
struct deserialize_t {
  omni::ryml::settings settings;

  /// Returns a new binding with transformed settings; preserves this binding.
  template <typename F>
  OMNI_CPP14_CONSTEXPR auto transform_settings(F &&f) const ->
    typename std::enable_if<
      compat::is_invocable<F &&, omni::ryml::settings>::value
        && std::is_same<omni::ryml::settings,
          compat::invoke_result_t<F &&, omni::ryml::settings>>::value,
      deserialize_t>::type {
    return {compat::invoke(std::forward<F>(f), omni::ryml::settings{settings})};
  }

  template <typename Record>
  with_diagnostics<Record, diagnostics<true>>
    operator()(omni::type_t<Record> record_type, std::string source) const {
    return core::deserialize(settings, record_type, std::move(source));
  }
};

OMNI_CPP14_CONSTEXPR const map_tree_t map_tree{};
OMNI_CPP14_CONSTEXPR const deserialize_t deserialize{};

// A generic callable keeps map_diagnostics(render_diagnostics) valid for both
// ownership modes; an overloaded function name cannot deduce its callable type.
struct render_diagnostics_t {
  template <bool Owning>
  std::string operator()(const diagnostics<Owning> &d) const;
};

/**
 * Render retained issues with RFC 6901 paths, separated by newlines.
 * Display the root as <root>; return an empty string when there are no issues.
 */
constexpr render_diagnostics_t render_diagnostics{};

/**
 * Serialize a reflected record, supported scalar, or vector to YAML or JSON.
 * Vectors may contain nested vectors or reflected records, including at the
 * root. String sources include char-based basic_string and c4 views, plus
 * basic_string_view when the standard library provides it.
 * Strings are quoted; field values are copied into the tree's arena.
 * TODO: Define the document representation of reflected enumerations.
 */
struct serialize_t {
  // Output format; use as_yaml or as_json for the predefined configurations.
  ::ryml::EmitType_e format;

  template <typename Value>
  std::string operator()(const Value &value) const;
};

/**
 * Serialize reflected records, supported scalars, or vectors to YAML text.
 *
```cpp
const auto yaml = as_yaml(value);
```
 */
constexpr serialize_t as_yaml{
  /*format=*/::ryml::EMIT_YAML,
};

/**
 * Serialize reflected records, supported scalars, or vectors to JSON text.
 *
```cpp
const auto json = as_json(value);
```
 */
constexpr serialize_t as_json{
  /*format=*/::ryml::EMIT_JSON,
};

} // namespace ryml
} // namespace omni

// Inline and template definitions depend on the complete public interface
// above. Including them here keeps the detail header private to this header.
#include <omnirefl/serialization/impl/ryml.hpp>
