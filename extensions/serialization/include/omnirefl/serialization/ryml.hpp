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
 * Serialization writes reflected records with as_yaml or as_json.
 * Compatible with C++11 and later.
 *
 * Examples prefer C++20 syntax. Compatibility interfaces are available for
 * C++11; see tests/extensions/serialization/test.cpp for complete usage.
 *
 * Given a reflectable person type and a std::string source:
 *
```cpp
auto result = omni::ryml::deserialize(omni::type<person>, source) //
  .transform_error(omni::ryml::render_diagnostics);

if (result) {
  const auto json = omni::ryml::as_json(*result);
  const auto yaml = omni::ryml::as_yaml(*result);
}
// On failure, result.error() contains the rendered diagnostics.
```
 */

/**
 * TODO: [high] Configure partial deserialization beyond a single bool.
 *
 * As of now, optional fields and containers default to warnings when
 * strategy.partial is true. Supported types are compat::optional (std or tl)
 * and std::vector, including issues within their nested values. Warnings are
 * retained and rendered, but do not spend tolerance or stop traversal.
 * With partial=false, invalid supplied values remain errors.
 *
 * Missing optional fields keep their initialized defaults without an issue;
 * explicit null clears them. Invalid optional values and wrong-kind containers
 * keep their initialized defaults. Sequences retain source indices, with
 * failed entries holding default values. Scalar and required-record issues
 * outside these fields remain errors; partial still permits their defaults
 * to be returned. Syntax errors always prevent returning a value.
 *
 * Consider runtime strategy settings for recovery and severity by field or
 * category: reject, omit, retain an initialized fallback, or default a value.
 * Decide whether failed sequence/mapping entries are skipped or retain their
 * positions, and whether optional values become empty or retain a fallback.
 * Consider finer recovery within optional records and containers rather than
 * rejecting an optional value whenever its contents report an issue.
 *
 * Default constructibility alone does not establish a valid fallback; consider
 * metadata for intended defaults and required fields. Keep severity, traversal
 * tolerance, value retention, and any warning collection limit independent.
 * Allow strict request handling and lenient config loading through runtime
 * configuration. Preserve C++11 support and lazy message rendering; defer
 * user-defined recovery and severity customization until these rules are clear.
 */

#include <omnirefl/reflected_scope.hpp>

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/functional.hpp>

#include <ryml.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace omni {
namespace ryml {

/**
 * Controls error collection, partial values, and acceptance of extra fields.
 * The partial setting's type selects the result type, not just its behavior.
 *
 * C++20 designated initialization with class template argument deduction:
 *
```cpp
const strategy strict{
  .tolerance = 4,
  .partial = std::false_type{},
  .extra = false,
}; // expected: value or diagnostics.
const strategy runtime{
  .tolerance = 4,
  .partial = false,
  .extra = false,
}; // Product: optional value and diagnostics together.
```
 *
 * C++11 compatibility builders preserve the same partial types:
 *
```cpp
const auto strict = use_tolerance(4) //
  .allow_partial(std::false_type{}); // expected: value or diagnostics.
const auto runtime = use_tolerance(4) //
  .allow_partial(false); // Product: optional value and diagnostics together.
```
 */
template <typename Partial = std::false_type>
struct strategy {
  static_assert(std::is_same<bool, Partial>::value
      || omni::traits::is<std::integral_constant, Partial>(),
    "Partial must be bool or std::integral_constant");

  // Number of errors traversal may pass before stopping on the next one.
  // Warnings do not count. Zero stops at the first error; negatives mean zero.
  // Increase this to collect more field errors even with partial=false.
  int tolerance = 0;

  // Permit returning a value despite errors; optional/container issues become
  // warnings. Failed, missing, and unvisited fields keep initialized defaults.
  // std::false_type gives expected; bool and std::true_type give
  // with_diagnostics. See deserialization_result for the full result types.
  Partial partial = {};

  // Ignore fields absent from the model at every nesting level.
  // Duplicates of model fields remain issues.
  bool extra = false;

  /**
   * Return a strategy with the new tolerance and other settings unchanged.
   *
   * Compatibility builder for code predating C++20 designated initializers.
   */
  OMNI_CPP14_CONSTEXPR strategy use_tolerance(int t) const {
    auto result = *this;
    result.tolerance = t;
    return result;
  }

  /**
   * Return a strategy with the partial type deduced from p and others
   * unchanged. Passing bool keeps the product result even when p is false;
   * passing std::false_type selects expected. See deserialization_result.
   *
   * Compatibility builder for code predating C++20 designated initializers.
   */
  template <typename P>
  OMNI_CPP14_CONSTEXPR strategy<P> allow_partial(P p) const {
    strategy<P> result{};
    result.tolerance = tolerance;
    result.partial = p;
    result.extra = extra;
    return result;
  }

  /**
   * Return a strategy with the new extra setting and others unchanged.
   *
   * Compatibility builder for code predating C++20 designated initializers.
   */
  OMNI_CPP14_CONSTEXPR strategy allow_extra(bool e) const {
    auto result = *this;
    result.extra = e;
    return result;
  }

  // TODO: [high] Add a configurable input-size limit to strategy and reject
  // oversized source text before parsing or allocating a tree. A std::string
  // argument is already buffered; callers must also bound request-body reads
  // to prevent oversized REST requests from consuming memory before this call.

  // TODO: Make strategy select SAX-style validation and field assignment as
  // rapidyaml emits events, or the current parse-whole-tree -> map pipeline.
  // Event-based mapping could reject invalid input before parsing and retaining
  // a huge remaining subtree. Preserve tolerance, partial/extra policies, and
  // diagnostics; check missing fields when their object closes. Resolve YAML
  // aliases and diagnostic-view ownership before enabling the SAX path.

  // TODO: [high] Detect at compile time whether a model permits unbounded
  // nesting (e.g. a vector of itself); consider a traversal-depth limit in
  // strategy to prevent stack exhaustion from deeply nested or malicious input.

  // TODO: [high] Define a union deserialization policy before mapping
  // std::variant. JSON Schema commonly represents alternatives with oneOf;
  // trying every alternative is ambiguous and may repeat expensive work.
  // Prefer a configurable discriminator that selects an alternative from a
  // field value. Decide how metadata names alternatives, where the
  // discriminator lives, and whether untagged matching is ever allowed.

  // TODO: Consider storing ryml::ParserOptions if rapidyaml's defaults prove
  // insufficient. Direct storage would make strategy non-constexpr; do not
  // mirror ParserOptions' private flags in another type.

  // TODO: [low] Consider a C++11-only constexpr constructor and reconstructing
  // values in the builders. Before C++14, member defaults prevent aggregate
  // initialization and constexpr functions cannot assign to a copied value.
};

/**
 * Start configuration with the given tolerance, partial of type
 * std::false_type, and extra=false. The partial type selects expected results.
 *
 * Compatibility builder for code predating C++20 designated initializers.
 */
inline OMNI_CPP14_CONSTEXPR strategy<> use_tolerance(int t) {
  strategy<> result{};
  result.tolerance = t;
  return result;
}

/**
 * Describes an input issue without rendering a message or deciding failure.
 */
struct issue {
  enum class code : std::uint8_t {
    unexpected_kind,
    invalid_scalar,
    out_of_range,
    missing_field,
    unknown_field,
    duplicate_field,
    parse_error
  };

  enum class kind : std::uint8_t {
    unknown,
    scalar,
    mapping,
    sequence,
    string,
    boolean,
    integer,
    unsigned_integer,
    number,
    null
  };

  struct path_segment {
    // Field name borrowed from metadata or the retained tree.
    c4::csubstr field;

    // Sequence position; empty for a named field.
    compat::optional<std::size_t> index;
  };

  // Group byte-sized kinds and the warning flag before rendering data.
  // Validation or conversion that failed.
  code reason;

  // Input kind required by the destination field.
  kind expected;

  // Input kind encountered at the failing node.
  kind actual;

  // True for recoverable optional/container issues under strategy.partial.
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
  // Preserves the call's settings.
  struct {
    // Number of errors traversal may pass before stopping on the next one.
    // Warnings do not count; negative call values are stored as zero.
    std::size_t tolerance;

    // Allows returning a value despite field errors, keeping their defaults.
    bool partial;

    // Ignores fields that are absent from the model.
    bool extra;
  } policy;

  // Retain the tree or refer to the caller's subtree, depending on Owning.
  // External string storage referenced by a caller-supplied tree must remain
  // alive and unchanged while using diagnostics, even when Owning is true.
  compat::conditional_t<Owning, ::ryml::Tree, ::ryml::ConstNodeRef> tree;

  // Validation and conversion issues in traversal order, including warnings.
  std::vector<issue> issues;

  // Owns the parser's text for a parse_error issue; mapping leaves it empty.
  std::string parse_message;

  // True when mapping's error budget requires stopping.
  bool stopped() const {
    return policy.tolerance < static_cast<std::size_t>( //
      std::count_if(issues.begin(),
        issues.end(),
        [](const issue &e) { return !e.warning; }));
  }
};

/**
 * Holds the deserialized value, if available, and diagnostics even on failure.
 * Diagnostics can hold issue records or a rendered message.
 */
template <typename T,
  typename Diagnostics = omni::ryml::diagnostics</*owning=*/ true>>
struct with_diagnostics {
  // The deserialized value, if available. strategy.partial controls
  // whether it can be returned despite field errors.
  compat::optional<T> value;

  // Retained issue records, or the result of transforming them for display.
  Diagnostics diagnostics;

  /**
   * Consume this result, applying f to an available value and keeping
   * diagnostics. When no value is available, do not call f.
   * Consumes an rvalue result; f must return a value, not void.
   */
  template <typename F>
  auto map_value(F &&f) && -> with_diagnostics<
    compat::decay_t<decltype(compat::invoke(std::forward<F>(f),
      std::move(*value)))>,
    Diagnostics> {
    if (value)
      return {
        /*value=*/compat::invoke(std::forward<F>(f), std::move(*value)),
        /*diagnostics=*/std::move(diagnostics),
      };

    return {
      /*value=*/compat::nullopt,
      /*diagnostics=*/std::move(diagnostics),
    };
  }

  /**
   * Consume this result, keeping the optional value and applying f to
   * diagnostics. Calls f whether or not a value is available, preserving
   * partial values. The returned diagnostics have the type returned by f.
   */
  template <typename F>
  auto map_diagnostics(F &&f) && -> with_diagnostics<T,
    compat::decay_t<decltype(compat::invoke(std::forward<F>(f),
      std::move(diagnostics)))>> {
    return {
      /*value=*/std::move(value),
      /*diagnostics=*/compat::invoke(std::forward<F>(f), std::move(diagnostics)),
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
   * C++20 designated initialization with class template argument deduction:
   *
  ```cpp
  const deserialize_t load{
    .strategy = strategy{
      .tolerance = 4,
      .partial = true,
      .extra = false,
    },
  };
  auto result = load(omni::type<person>, source);
  auto rendered = std::move(result) //
    .transform_error(render_diagnostics);
  // rendered.value keeps the partial person; rendered.diagnostics is text.
  ```
   *
   * In C++11, initialize load with the compatibility builder and use
   * omni::type_t<person>{} for the type argument:
   *
  ```cpp
  const auto load = fn::ctad<deserialize_t>()(
    use_tolerance(4) //
      .allow_partial(true));
  ```
   */
  template <typename F>
  auto transform_error(
    F &&f) && -> decltype(std::move(*this).map_error(std::forward<F>(f))) {
    return std::move(*this) //
      .map_error(std::forward<F>(f));
  }

  // TODO: Add and_then for generic expected-style sequencing; specify how to
  // retain diagnostics from both the input and the returned result.
  // TODO: Add or_else for generic expected-style recovery; specify how to
  // retain diagnostics when a value and issues are both present.
};

// True when deserialization returned a value with field issues.
template <typename T, bool Owning>
bool is_partial(const with_diagnostics<T, diagnostics<Owning>> &r) {
  return r.value && !r.diagnostics.issues.empty();
}

namespace detail {

template <typename Strategy>
struct returns_expected;

template <>
struct returns_expected<strategy<bool>>: std::false_type {};

template <typename T, T Value>
struct returns_expected<strategy<std::integral_constant<T, Value>>>:
  std::integral_constant<bool, !static_cast<bool>(Value)> {};

} // namespace detail

/**
 * Result of deserializing T using Strategy and owning or borrowed diagnostics.
 *
 * strategy<std::false_type> gives expected<T, diagnostics<Owning>>:
 * a value on success or diagnostics on failure. Other std::integral_constant
 * types whose value is false give the same result type.
 *
 * strategy<bool> and strategy<std::true_type> give with_diagnostics<T,
 * diagnostics<Owning>>: an optional value and diagnostics together.
 * For strategy<bool>, the result type stays the same whether partial is true
 * or false at runtime.
 */
template <typename T, typename Strategy, bool Owning>
using deserialization_result = compat::conditional_t< //
  detail::returns_expected<Strategy>::value,
  compat::expected<T, diagnostics<Owning>>,
  with_diagnostics<T, diagnostics<Owning>>>;

/**
 * Parse text into an owning tree, returning syntax errors without throwing.
 *
 * ParserOptions are passed directly to ryml. Location tracking, if enabled,
 * ends with this call; the returned tree does not retain the parser.
 *
```cpp
// Supply a reflected person and an owning std::string source.
auto result = parse(std::move(source)) //
  .and_then([](::ryml::Tree tree) {
    return map_tree(omni::type<person>, std::move(tree)) //
      .transform_error(render_diagnostics);
  });
// expected<person, std::string>: syntax and field errors are rendered text.
```
 */
inline compat::expected<::ryml::Tree, std::string> parse(std::string source,
  ::ryml::ParserOptions options = {});

/**
 * Map an owning tree or borrowed node view using the stored strategy.
 */
template <typename Strategy>
struct map_tree_t {
  static_assert(omni::traits::is<omni::ryml::strategy, Strategy>(),
    "Strategy must be an omni::ryml::strategy specialization");

  // Settings applied to every tree or subtree mapped by this callable.
  Strategy strategy;

  /**
   * Own a copy for an lvalue tree, or transfer a moved tree after traversal.
   * Returns expected when partial has type std::false_type; bool and
   * std::true_type return with_diagnostics. See deserialization_result.
   */
  template <typename To>
  deserialization_result<To, Strategy, /*owning=*/ true>
    operator()(omni::type_t<To> to, ::ryml::Tree tree) const;

  /**
   * Borrow the subtree. Keep the caller's tree alive and unchanged while using
   * the returned diagnostics. Returns expected when partial has type
   * std::false_type; bool and std::true_type return with_diagnostics.
   * See deserialization_result.
   */
  template <typename To>
  deserialization_result<To, Strategy, /*owning=*/ false>
    operator()(omni::type_t<To> to, ::ryml::ConstNodeRef from) const;
};

#if defined(__cpp_deduction_guides) && 201703L <= __cpp_deduction_guides
template <typename Strategy>
map_tree_t(Strategy) -> map_tree_t<Strategy>;
#endif

/**
 * Map with zero tolerance, partial values disabled, and extra fields rejected.
 * Returns expected with owning diagnostics for trees, borrowed for node views.
 *
```cpp
auto borrowed = map_tree(omni::type<person>, borrowed_tree.crootref());
// Keep borrowed_tree and its strings alive and unchanged while using errors.
auto owned = map_tree(omni::type<person>, std::move(owning_tree));
// Errors retain the moved tree; external backing strings still need an owner.
```
 */
const auto map_tree = fn::ctad<map_tree_t>()(
  /*strategy=*/use_tolerance(0) //
    .allow_partial(std::false_type{}));

/**
 * Compose parsing and tree mapping, retaining all issues in diagnostics.
 * Parse errors always return no value; strategy.partial applies to mapping.
 * Returns expected when partial has type std::false_type; bool and
 * std::true_type return with_diagnostics. See deserialization_result.
 */
template <typename Strategy>
struct deserialize_t {
  static_assert(omni::traits::is<omni::ryml::strategy, Strategy>(),
    "Strategy must be an omni::ryml::strategy specialization");

  // Settings applied to every deserialization performed by this callable.
  Strategy strategy;

  template <typename To>
  deserialization_result<To, Strategy, /*owning=*/ true> operator()(
    omni::type_t<To> to,
    std::string source) const;
};

#if defined(__cpp_deduction_guides) && 201703L <= __cpp_deduction_guides
template <typename Strategy>
deserialize_t(Strategy) -> deserialize_t<Strategy>;
#endif

/**
 * Parse and map with zero tolerance, no partial values, and no extra fields.
 * Returns expected: the deserialized value or diagnostics that own the parsed
 * tree.
 *
 * Increase tolerance to collect more field errors while keeping partial=false.
 *
```cpp
auto result = deserialize(omni::type<person>, source) //
  .transform_error(render_diagnostics);
// expected<person, std::string>: rendering happens only on failure.
```
 */
const auto deserialize = fn::ctad<deserialize_t>()(
  /*strategy=*/use_tolerance(0) //
    .allow_partial(std::false_type{}));

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
 * Serialize a reflected record and its nested fields to YAML or JSON.
 * Strings are quoted; field values are copied into the tree's arena.
 */
struct serialize_t {
  // Output format; use as_yaml or as_json for the predefined configurations.
  ::ryml::EmitType_e format;

  template <typename From>
  std::string operator()(const From &from) const;
};

/**
 * Serialize reflected records to YAML text.
 * This call is unchanged in C++11.
 *
```cpp
const auto yaml = as_yaml(value);
```
 */
constexpr serialize_t as_yaml{
  /*format=*/::ryml::EMIT_YAML,
};

/**
 * Serialize reflected records to JSON text.
 * This call is unchanged in C++11.
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
