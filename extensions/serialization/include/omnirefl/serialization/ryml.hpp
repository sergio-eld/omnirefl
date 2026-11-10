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
  .transform_diagnostics(omni::ryml::render_diagnostics);

if (result.value) {
  const auto json = omni::ryml::as_json(*result.value);
  const auto yaml = omni::ryml::as_yaml(*result.value);
}
// result.diagnostics contains the rendered diagnostics.
```
 */

#include <omnirefl/reflected_scope.hpp>
#include <omnirefl/functional.hpp>

#include <omnirefl/extensions/compat.hpp>

#include <ryml.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
// FIXME: [low] Freestanding: public diagnostics and text APIs require hosted
// owning strings and vectors; provide an alternative representation.
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#if 201703L <= OMNI_CPLUSPLUS && defined(__has_include)
#  if __has_include(<string_view>)
#    include <string_view>
#  endif
#endif

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

/** Configures input transformation, mapping diagnostics, and missing-field
 * recovery. */
struct config {
  json::error_policy error_policy;

  // No missing-field recovery unless explicitly configured.
  compat::optional<json::recovery_policy> recovery = compat::nullopt;

  // Tree -> Tree, applied after successful parsing and before mapping.
  // An empty callback leaves the tree unchanged. The parsed tree owns its
  // text; the transformed tree is retained by the returned diagnostics.
  std::function<::ryml::Tree(::ryml::Tree)> transform_tree{};

  // TODO: [high] Add a configurable input-size limit to config and reject
  // oversized source text before parsing or allocating a tree. A std::string
  // argument is already buffered; callers must also bound request-body reads
  // to prevent oversized REST requests from consuming memory before this call.

  // TODO: Make config select SAX-style validation and field assignment as
  // rapidyaml emits events, or the current parse-whole-tree -> map pipeline.
  // Event-based mapping could reject invalid input before parsing and retaining
  // a huge remaining subtree. Preserve error and recovery policies, and
  // diagnostics; check missing fields when their object closes. Resolve YAML
  // aliases and diagnostic-view ownership before enabling the SAX path.

  // TODO: [high] Detect at compile time whether a model permits unbounded
  // nesting (e.g. a vector of itself); consider a traversal-depth limit in
  // config to prevent stack exhaustion from deeply nested or malicious input.

  // TODO: [high] Define a union deserialization policy before mapping
  // std::variant. JSON Schema commonly represents alternatives with oneOf;
  // trying every alternative is ambiguous and may repeat expensive work.
  // Prefer a configurable discriminator that selects an alternative from a
  // field value. Decide how metadata names alternatives, where the
  // discriminator lives, and whether untagged matching is ever allowed.

  // TODO: Consider storing ryml::ParserOptions if rapidyaml's defaults prove
  // insufficient; do not mirror ParserOptions' private flags in another type.
};

// TODO: Consider global-default customization without requiring users to name
// config before this header has declared it; config has no separate header.

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
 * When OwnsTree is false, keep the caller's tree alive and unchanged while using
 * diagnostics. Moving or changing it can invalidate node and string views.
 * When true, move this object; copying does not rebind issue views to its copy.
 * The copy constructor is left implicit to keep initialization simple and avoid
 * explicit copy/move special-member declarations.
 */
template <bool OwnsTree>
struct diagnostics {
  // Snapshot for inspection; mapping receives its config separately.
  omni::ryml::config config;

  // Retain the tree or refer to the caller's subtree, depending on OwnsTree.
  // External string storage referenced by a caller-supplied tree must remain
  // alive and unchanged while using diagnostics, even when OwnsTree is true.
  compat::conditional_t<OwnsTree, ::ryml::Tree, ::ryml::ConstNodeRef> tree;

  // Validation and conversion issues in traversal order, including warnings.
  std::vector<issue> issues;

  // Owns the parser's text for a parse_error issue; mapping leaves it empty.
  std::string parse_message;

  // True when an error or warning limit requires stopping.
  bool stopped() const {
    return config.error_policy.error_limit
        < static_cast<std::size_t>(std::count_if(issues.begin(),
          issues.end(),
          [](const issue &e) { return !e.warning; }))
      || std::any_of(issues.begin(), issues.end(),
        [](const issue &e) { return json::issue_code::warning_limit_exceeded == e.reason; });
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
  auto transform_value(F &&f) &&
    -> with_diagnostics<compat::decay_t<compat::invoke_result_t<F &&, T &&>>,
      Diagnostics> {
    return {
      /*value=*/value
        ? compat::make_optional(
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
  auto transform_diagnostics(F &&f) &&
    -> with_diagnostics<T,
      compat::decay_t<compat::invoke_result_t<F &&, Diagnostics &&>>> {
    return {
      /*value=*/std::move(value),
      /*diagnostics=*/
      compat::invoke(std::forward<F>(f), std::move(diagnostics)),
    };
  }

  // TODO: Collect defaulted fields separately from warnings and errors.
  // TODO: Add and_then for generic expected-style sequencing; specify how to
  // retain diagnostics from both the input and the returned result.
  // TODO: Add or_else for generic expected-style recovery; specify how to
  // retain diagnostics when a value and issues are both present.
};

namespace detail {
struct mapping_context;
} // namespace detail

/** Mapping protocols may be specialized for user-defined types. */

// Specialize for custom strings exposing value_type, data(), size(), and a
// (data, size) constructor. String-view destinations borrow the input tree.
template <typename>
struct is_string_like: std::false_type {};

template <typename Char, typename Traits, typename Allocator>
struct is_string_like<std::basic_string<Char, Traits, Allocator>>:
    std::true_type {};

template <typename Char>
struct is_string_like<c4::basic_substring<Char>>: std::true_type {};

#if defined(__cpp_lib_string_view)
template <typename Char, typename Traits>
struct is_string_like<std::basic_string_view<Char, Traits>>: std::true_type {};
#endif

// Opt in standard-like optional wrappers. Mapping uses value_type, bool
// conversion, dereference, reset(), and emplace(value). Empty optional sequence
// elements are omitted. Raw and smart pointers are not optional-like.
template <typename>
struct is_optional_like: std::false_type {};

#if defined(__cpp_lib_optional) && 201606L <= __cpp_lib_optional
template <typename T>
struct is_optional_like<std::optional<T>>: std::true_type {};
#endif

#if defined(TL_OPTIONAL_HPP)
template <typename T>
struct is_optional_like<tl::optional<T>>: std::true_type {};
#endif

template <typename T, typename = void>
struct is_container: std::false_type {};

template <typename T>
struct is_container<T,
  compat::void_t<typename T::value_type,
    decltype(std::declval<const T &>().begin()),
    decltype(std::declval<const T &>().end())>>:
    std::integral_constant<bool, !is_string_like<T>::value> {};

namespace cp {

/** Container insertion customization, independent of representation mapping. */
template <typename Container, typename Enable = void>
struct emplace {
  void operator()(Container *into,
    typename Container::value_type &&value) const;
  // TODO: Support standard-container insertion, preferably through traits.
};

/** Select a C++-to-node adapter; the default maps reflected records. */
template <typename T, typename Enable = void>
struct from_type;

/** Select a node-to-C++ adapter; return true on successful conversion. */
template <typename T, typename Enable = void>
struct to_type;

} // namespace cp

namespace core {

namespace detail {
template <typename T>
struct reflection_root;
} // namespace detail

/** Dispatch one value through cp::from_type unless diagnostics have stopped. */
template <typename T>
void write_node(const T &value, ::ryml::NodeRef node,
  omni::ryml::detail::mapping_context &diagnostics);

/** Dispatch one node through cp::to_type and return mapping success. */
template <typename T>
bool read_node(::ryml::ConstNodeRef node, T *into,
  omni::ryml::detail::mapping_context &diagnostics);

/** Map into an existing value using a shared diagnostics context. */
template <typename T>
auto map_tree(::ryml::ConstNodeRef node, T *into,
  omni::ryml::detail::mapping_context &diagnostics)
  -> compat::enable_if_t<detail::reflection_root<T>::needs_reflection, bool>;

template <typename T>
auto map_tree(::ryml::ConstNodeRef node, T *into,
  omni::ryml::detail::mapping_context &diagnostics)
  -> compat::enable_if_t<!detail::reflection_root<T>::needs_reflection, bool>;

/**
 * Non-throwing adapter for RapidYAML parsing errors. Parses text into an
 * owning tree and returns syntax errors through expected.
 * ParserOptions are passed directly to ryml. Location tracking, if enabled,
 * ends with this call; the returned tree does not retain the parser.
 *
 * TODO: Remove this adapter when RapidYAML provides native non-throwing parsing
 * with a result-based error interface.
 *
```cpp
// Supply a reflected person, config, and an owning std::string source.
auto result = core::parse(std::move(source))
  .transform([options](::ryml::Tree tree) {
    return core::map_tree(options, omni::type_t<person>{}, std::move(tree));
  });
// Syntax errors use expected; mapping retains optional person and diagnostics.
```
 */
inline compat::expected<::ryml::Tree, std::string> parse(std::string source,
  ::ryml::ParserOptions options = {});

/** Map tree nodes into the requested type using explicit config.
 * Nested values are mapped depth-first through the selected customization
 * points.
 * Diagnostics take ownership of the supplied tree, retaining it for inspection
 * and lazy message rendering. Pass std::move(tree) to transfer an existing
 * tree.
 */
template <typename Record>
with_diagnostics<Record, diagnostics<true>> map_tree(const config &options,
  omni::type_t<Record> record_type,
  ::ryml::Tree tree);

/** Map tree nodes into the requested type using explicit config.
 * Nested values are mapped depth-first through the selected customization
 * points.
 * Diagnostics borrow the source tree rather than taking ownership.
 * Keep the source tree and its borrowed string storage alive and unchanged
 * while inspecting the retained diagnostics or rendering their messages.
 */
template <typename Record>
with_diagnostics<Record, diagnostics<false>> map_tree(const config &options,
  omni::type_t<Record> record_type,
  ::ryml::ConstNodeRef node);

/** Parse the source, apply its configured tree transformation, then map nodes.
 * The transformation is invoked only after successful parsing.
 */
template <typename Record>
with_diagnostics<Record, diagnostics<true>> deserialize(const config &options,
  omni::type_t<Record> record_type,
  std::string source);

/** Build an owning RapidYAML tree from reflected records, supported scalars,
 * vectors, and optionals. Empty optionals produce null nodes; empty optional
 * sequence elements are omitted.
 * String data is copied into the tree's arena, so the tree can outlive the
 * source value. The returned tree can be modified or passed to RapidYAML's
 * emitters.
 */
template <typename Value>
auto as_owning_tree(const Value &v)
  -> compat::enable_if_t<detail::reflection_root<Value>::needs_reflection,
    ::ryml::Tree>;

template <typename Value>
auto as_owning_tree(const Value &v)
  -> compat::enable_if_t<!detail::reflection_root<Value>::needs_reflection,
    ::ryml::Tree>;

/** Build a RapidYAML tree borrowing the input's existing string storage.
 * The tree owns its nodes and generated scalar text. Borrowed storage must
 * remain alive and unchanged while the tree is used. Accepts const or mutable
 * lvalues; temporaries are rejected to prevent dangling views.
 */
template <typename Value>
auto as_ref_tree(Value &v)
  -> compat::enable_if_t<
    detail::reflection_root<compat::remove_cvref_t<Value>>::needs_reflection,
    ::ryml::Tree>;

template <typename Value>
auto as_ref_tree(Value &v)
  -> compat::enable_if_t<
    !detail::reflection_root<compat::remove_cvref_t<Value>>::needs_reflection,
    ::ryml::Tree>;

// Also reject const temporaries, which could otherwise bind through Value&.
template <typename Value>
::ryml::Tree as_ref_tree(const Value &&) = delete;

} // namespace core

/// Maps an owning tree or borrowed subtree with the bound config.
/// Supports reflected records, scalars, and vectors of supported values.
struct map_tree_t {
  omni::ryml::config config;

  /// Returns a new binding with transformed config; preserves this binding.
  template <typename F>
  auto transform_config(F &&f) const
    -> compat::enable_if_t<
      compat::is_invocable<F &&, omni::ryml::config>::value
        && std::is_same<omni::ryml::config,
          compat::invoke_result_t<F &&, omni::ryml::config>>::value,
      map_tree_t> {
    return {
      compat::invoke(std::forward<F>(f), omni::ryml::config{this->config}),
    };
  }

  // Own the tree; move it after mapping so diagnostic views remain valid.
  template <typename Record>
  with_diagnostics<Record, diagnostics<true>>
    operator()(omni::type_t<Record> record_type, ::ryml::Tree tree) const {
    return core::map_tree(this->config, record_type, std::move(tree));
  }

  // Keep the caller's tree alive and unchanged while using borrowed
  // diagnostics.
  template <typename Record>
  with_diagnostics<Record, diagnostics<false>> operator()(
    omni::type_t<Record> record_type,
    ::ryml::ConstNodeRef node) const {
    return core::map_tree(this->config, record_type, node);
  }
};

// Previous selects (Tree, previous) -> Tree instead of Tree -> Tree.
template <typename F, bool Previous = false, typename = void>
struct is_tree_transform: std::false_type {};

// SFINAE keeps unsupported invocation signatures queryable as false.
template <typename F>
struct is_tree_transform<F,
  false,
  compat::void_t<compat::invoke_result_t<F &, ::ryml::Tree>>>:
    compat::conjunction<std::is_copy_constructible<F>,
      std::is_same<::ryml::Tree,
        compat::remove_cvref_t<compat::invoke_result_t<F &, ::ryml::Tree>>>> {};

template <typename F>
struct is_tree_transform<F,
  true,
  compat::void_t<compat::invoke_result_t<F &,
    ::ryml::Tree,
    const std::function<::ryml::Tree(::ryml::Tree)> &>>>:
    compat::conjunction<std::is_copy_constructible<F>,
      std::is_same<::ryml::Tree,
        compat::remove_cvref_t<compat::invoke_result_t<F &,
          ::ryml::Tree,
          const std::function<::ryml::Tree(::ryml::Tree)> &>>>> {};

/** Return a config transformation that replaces the parsed-tree callback.
 * f must be copy constructible and return ryml::Tree (cv/ref-qualified is OK).
 * A null function pointer resets the callback to identity. Applying this
 * transformation preserves the other config fields.
 */
template <typename F,
  compat::enable_if_t<!is_tree_transform<F, /*Previous=*/true>::value
      && is_tree_transform<F>::value,
    int> = 0>
OMNI_CPP17_CONSTEXPR auto with_transform_tree(F f)
  OMNI_CPP11_TRAILING_RETURN(std::function<config(config)>) {
  return [f](config c) {
    c.transform_tree = f;
    return c;
  };
}

/** Return a config transformation that decorates the parsed-tree callback.
 * f receives (tree, previous); previous is identity when no callback was bound.
 * f may invoke or discard previous, whose reference is valid during the call.
 * Copy previous if retaining it. f must be copy constructible and return a
 * tree (cv/ref-qualified is OK). A null function pointer resets to identity.
 * This form takes precedence when f also accepts a single tree argument.
 */
template <typename F,
  compat::enable_if_t<is_tree_transform<F, /*Previous=*/true>::value, int> = 0>
OMNI_CPP17_CONSTEXPR auto with_transform_tree(F f)
  OMNI_CPP11_TRAILING_RETURN(std::function<config(config)>) {
  return [f](config c) {
    const bool reset = fn::is_null_function(f);
    const auto previous = !reset && c.transform_tree
      ? c.transform_tree
      : fn::identity{};

    c.transform_tree = reset
      ? previous
      : [f, previous](::ryml::Tree tree) mutable {
          return compat::invoke(f, std::move(tree), previous);
        };

    return c;
  };
}

/**
 * Parse JSON or YAML into a record, scalar, or vector.
 * Returns a valid value with any warnings, or diagnostics alone on failure.
 *
```cpp
const auto load = deserialize.transform_config([limit](ryml::config s) {
  s.error_policy.error_limit = limit;
  s.error_policy.unknown_fields = json::unknown_policy::allow;
  return s;
});
auto result = load(omni::type_t<person>{}, source);
```
 */
struct deserialize_t {
  omni::ryml::config config;

  /// Returns a new binding with transformed config; preserves this binding.
  template <typename F>
  auto transform_config(F &&f) const
    -> compat::enable_if_t<
      compat::is_invocable<F &&, omni::ryml::config>::value
        && std::is_same<omni::ryml::config,
          compat::invoke_result_t<F &&, omni::ryml::config>>::value,
      deserialize_t> {
    return {
      /*config=*/
      compat::invoke(std::forward<F>(f), omni::ryml::config{this->config}),
    };
  }

  template <typename Record>
  with_diagnostics<Record, diagnostics<true>>
    operator()(omni::type_t<Record> record_type, std::string source) const {
    return core::deserialize(this->config, record_type, std::move(source));
  }
};

const map_tree_t map_tree{};
const deserialize_t deserialize{};

// A generic callable lets transform_diagnostics(render_diagnostics) work with
// both ownership modes; an overloaded function name cannot deduce its type.
struct render_diagnostics_t {
  template <bool OwnsTree>
  std::string operator()(const diagnostics<OwnsTree> &d) const;
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
 * Empty optionals emit null at the root or as record fields. Empty optional
 * sequence elements are omitted; remaining elements keep their relative order.
 * Populated optionals serialize their contained value.
 * Strings are quoted and borrowed from the input until emission completes.
 * Reflected enums serialize as their declared names.
 */
struct serialize_t {
  // Output format; use as_yaml or as_json for the predefined configurations.
  ::ryml::EmitType_e format;

  template <typename Value>
  std::string operator()(const Value &value) const;

  // TODO: [high] Consider transform_tree to customize the built tree before
  // emission, e.g. omit null object properties without another policy.
};

/**
 * Serialize reflected records, supported scalars, or vectors to YAML text.
 * Reads the value and its referenced storage through const references. Keep
 * them alive and unchanged until the call returns. Concurrent reads are safe;
 * synchronize any concurrent mutation or destruction of the input storage.
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
 * Reads the value and its referenced storage through const references. Keep
 * them alive and unchanged until the call returns. Concurrent reads are safe;
 * synchronize any concurrent mutation or destruction of the input storage.
 *
```cpp
const auto json = as_json(value);
```
 */
constexpr serialize_t as_json{
  /*format=*/::ryml::EMIT_JSON,
};

namespace mapping {

/** Write a char-based string as a quoted scalar. */
struct from_string_like {
  template <typename T>
  static constexpr bool supports() { return is_string_like<T>::value; }

  template <typename T>
  auto operator()(const T &value,
    ::ryml::NodeRef node,
    omni::ryml::detail::mapping_context &) const
    -> compat::enable_if_t<supports<T>()>;
};

/** Read scalar text into a string-like type; views borrow tree storage. */
struct to_string_like {
  template <typename T>
  static constexpr bool supports() { return is_string_like<T>::value; }

  template <typename T>
  auto operator()(::ryml::ConstNodeRef node,
    T *into,
    omni::ryml::detail::mapping_context &diagnostics) const
    -> compat::enable_if_t<supports<T>(), bool>;
};

/** Write booleans, integers, and floating-point values as plain scalars. */
struct from_arithmetic {
  template <typename T>
  static constexpr bool supports() { return std::is_arithmetic<T>::value; }

  template <typename T>
  auto operator()(const T &value,
    ::ryml::NodeRef node,
    omni::ryml::detail::mapping_context &) const
    -> compat::enable_if_t<supports<T>()>;
};

/** Validate scalar format and range before assigning an arithmetic value. */
struct to_arithmetic {
  template <typename T>
  static constexpr bool supports() { return std::is_arithmetic<T>::value; }

  template <typename T>
  auto operator()(::ryml::ConstNodeRef node,
    T *into,
    omni::ryml::detail::mapping_context &diagnostics) const
    -> compat::enable_if_t<supports<T>(), bool>;
};

/** Write a present optional value or a null scalar. */
struct from_optional_like {
  template <typename T>
  static constexpr bool supports() { return is_optional_like<T>::value; }

  template <typename T>
  auto operator()(const T &value,
    ::ryml::NodeRef node,
    omni::ryml::detail::mapping_context &diagnostics) const
    -> compat::enable_if_t<supports<T>()>;
};

/** Reset on null; otherwise convert and emplace the contained value. */
struct to_optional_like {
  template <typename T>
  static constexpr bool supports() { return is_optional_like<T>::value; }

  template <typename T>
  auto operator()(::ryml::ConstNodeRef node,
    T *into,
    omni::ryml::detail::mapping_context &diagnostics) const
    -> compat::enable_if_t<supports<T>(), bool>;
};

/** Write iterable elements as a sequence, omitting empty optionals. */
struct from_container {
  template <typename T>
  static constexpr bool supports() { return is_container<T>::value; }

  template <typename T>
  auto operator()(const T &value,
    ::ryml::NodeRef node,
    omni::ryml::detail::mapping_context &diagnostics) const
    -> compat::enable_if_t<supports<T>()>;
};

/** Read sequence elements and insert successful values through cp::emplace. */
struct to_container {
  template <typename T>
  static constexpr bool supports() { return is_container<T>::value; }

  template <typename T>
  auto operator()(::ryml::ConstNodeRef node,
    T *into,
    omni::ryml::detail::mapping_context &diagnostics) const
    -> compat::enable_if_t<supports<T>(), bool>;
};

namespace reflected_scope {

/** Write a declared enumerator name as a quoted scalar. */
struct from_enum {
  template <typename T>
  static constexpr bool supports() {
    return omni::is_reflected_enum<T>::value;
  }

  template <typename T>
  auto operator()(T value,
    ::ryml::NodeRef node,
    omni::ryml::detail::mapping_context &) const
    -> compat::enable_if_t<supports<T>()>;
};

/** Accept a string matching a declared enumerator name. */
struct to_enum {
  template <typename T>
  static constexpr bool supports() {
    return omni::is_reflected_enum<T>::value;
  }

  template <typename T>
  auto operator()(::ryml::ConstNodeRef node,
    T *into,
    omni::ryml::detail::mapping_context &diagnostics) const
    -> compat::enable_if_t<supports<T>(), bool>;
};

/** Write accessible non-static data members as mapping entries. */
struct from_record {
  template <typename T>
  static constexpr bool supports() {
    return omni::is_reflected_record<T>::value;
  }

  template <typename T>
  auto operator()(const T &value,
    ::ryml::NodeRef node,
    omni::ryml::detail::mapping_context &diagnostics) const
    -> compat::enable_if_t<supports<T>()>;
};

/** Read accessible non-static data members from matching mapping entries. */
struct to_record {
  template <typename T>
  static constexpr bool supports() {
    return omni::is_reflected_record<T>::value;
  }

  template <typename T>
  auto operator()(::ryml::ConstNodeRef node,
    T *into,
    omni::ryml::detail::mapping_context &diagnostics) const
    -> compat::enable_if_t<supports<T>(), bool>;
};

} // namespace reflected_scope

} // namespace mapping

namespace cp {

// Record mapping is the fallback; matching specializations replace it.
template <typename T, typename Enable>
struct from_type: mapping::reflected_scope::from_record {};

template <typename T, typename Enable>
struct to_type: mapping::reflected_scope::to_record {};

template <typename T>
struct from_type<T,
  compat::enable_if_t<mapping::from_string_like::supports<T>()>>:
    mapping::from_string_like {};

template <typename T>
struct to_type<T,
  compat::enable_if_t<mapping::to_string_like::supports<T>()>>:
    mapping::to_string_like {};

template <typename T>
struct from_type<T,
  compat::enable_if_t<mapping::from_arithmetic::supports<T>()>>:
    mapping::from_arithmetic {};

template <typename T>
struct to_type<T,
  compat::enable_if_t<mapping::to_arithmetic::supports<T>()>>:
    mapping::to_arithmetic {};

template <typename T>
struct from_type<T,
  compat::enable_if_t<mapping::from_optional_like::supports<T>()>>:
    mapping::from_optional_like {};

template <typename T>
struct to_type<T,
  compat::enable_if_t<mapping::to_optional_like::supports<T>()>>:
    mapping::to_optional_like {};

template <typename T>
struct from_type<T,
  compat::enable_if_t<mapping::from_container::supports<T>()>>:
    mapping::from_container {};

template <typename T>
struct to_type<T,
  compat::enable_if_t<mapping::to_container::supports<T>()>>:
    mapping::to_container {};

template <typename T>
struct from_type<T,
  compat::enable_if_t<mapping::reflected_scope::from_enum::supports<T>()>>:
    mapping::reflected_scope::from_enum {};

template <typename T>
struct to_type<T,
  compat::enable_if_t<mapping::reflected_scope::to_enum::supports<T>()>>:
    mapping::reflected_scope::to_enum {};

} // namespace cp

} // namespace ryml
} // namespace omni

// Inline and template definitions depend on the complete public interface
// above. Including them here keeps the detail header private to this header.
#include <omnirefl/serialization/impl/ryml.hpp>
