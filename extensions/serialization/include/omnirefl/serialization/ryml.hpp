#pragma once

// TODO: [medium] Add const & and && overloads for functional-update members
// such as tdm_transform_diagnostics and tdm_config. Copy retained state
// from the global const defaults; move it when chaining returned temporaries
// or calling std::move(local_config).with_...(). Review copy-constructible
// requirements so rvalue updates can support move-only retained state where
// the underlying operation permits it.

// TODO: [high] Define how deserialize can return values borrowing tree storage.
// Such values need an external owner; map_tree lets the caller retain its tree.

// TODO: [high] Document that users must not modify source strings or string
// containers during serialization calls. Borrowed character storage must stay
// valid and unchanged until emission completes, including mutable members and
// storage exposed by custom iterators; a const reference cannot guarantee this.

#if !defined(OMNI_TOOL_RUN) \
  && !defined(OMNI_INCLUDED_GENERATED_REFLECTION_HEADER)
#  error \
    "Include the generated reflection header before this serialization header. " \
    "With CMake, call omni_reflected_target(<target>)."
#endif

/**
 * YAML and JSON serialization backed by rapidyaml and omnirefl metadata.
 * Deserialization composes parsing, tree validation, and reflected field
 * assignment with diagnostics transformed by the supplied configuration.
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
auto result = omni::ryml::deserialize(omni::type<person>, source);

if (result.value) {
  const auto json = omni::ryml::as_json(*result.value);
  const auto yaml = omni::ryml::as_yaml(*result.value);
}
// result.diagnostics contains the rendered diagnostics.
``` */

#include <omnirefl/reflected_scope.hpp>
#include <omnirefl/functional.hpp>
#include <omnirefl/traits.hpp>

#include <omnirefl/extensions/traits.hpp>
#include <omnirefl/serialization/ryml/mapping.hpp>

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

#if 201703L <= OMNI_CPLUSPLUS && defined(__has_include)
#  if __has_include(<string_view>)
#    include <string_view>
#  endif
#endif

namespace omni {
namespace traits {
namespace cp {

// c4 views use the same operation specialization points as user-defined strings.
template <>
struct as_string<c4::csubstr> {
  constexpr compat::string_view operator()(c4::csubstr value) const {
    return {value.str, value.len};
  }
};

template <>
struct assign_string<c4::csubstr> {
  void operator()(c4::csubstr *into, compat::string_view text) const {
    *into = {text.data(), text.size()};
  }
};

} // namespace cp
} // namespace traits

// Only backend-independent JSON/YAML mapping contracts belong here.
// TODO: Move to a shared header if another JSON parser is adopted.
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
  warning_limit_exceeded,
  duplicate_element // Repeated value in a unique-element destination.
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

/// Controls repeated object names and elements of unique destinations.
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

} // namespace json

namespace ryml {

struct diagnostics;

/** Render issue records using their source tree while it is alive. */
struct render_diagnostics_t {
  std::string operator()(const ::ryml::Tree &tree, const diagnostics &d) const;
};

constexpr render_diagnostics_t render_diagnostics{};

/** Deserialization configuration.
 * In C++20, aggregate deduction supports config{.transform_diagnostics = f}.
 * Earlier standards can use tdm_transform_diagnostics to change the callable
 * type without spelling the resulting config specialization. */
template <typename TransformDiagnostics = render_diagnostics_t>
struct config {
  json::error_policy error_policy OMNI_CPP14_DEFAULT_INIT();

  // Called with the live source tree and collected diagnostics on every result.
  TransformDiagnostics transform_diagnostics OMNI_CPP14_DEFAULT_INIT();

  /** Transform data member `.transform_diagnostics` inplace, in one expression.
   * Invokes f immediately with the current callable and returns a new config
   * containing its result, preserving error_policy and this config.
   * f :: TransformDiagnostics -> NewTransformDiagnostics.
   *
```cpp
const auto load = omni::ryml::deserialize.tdm_config(
  [](omni::ryml::config<> options) {
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
    (compat::is_invocable<F &, TransformDiagnostics>::value))
  constexpr auto tdm_transform_diagnostics(F &&f) const
    -> config<compat::decay_t<
      compat::invoke_result_t<F &, TransformDiagnostics>>> {
    return {
      this->error_policy,
      /*transform_diagnostics=*/compat::invoke(f,
        TransformDiagnostics{this->transform_diagnostics}),
    };
  }

  // TODO: [high] Add a configurable input-size limit to config and reject
  // oversized source text before parsing or allocating a tree. A std::string
  // argument is already buffered; callers must also bound request-body reads
  // to prevent oversized REST requests from consuming memory before this call.

  // TODO: Make config select SAX-style validation and field assignment as
  // rapidyaml emits events, or the current parse-whole-tree -> map pipeline.
  // Event-based mapping could reject invalid input before parsing and retaining
  // a huge remaining subtree. Preserve mapping policies and
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
  // TODO: Revisit path storage if C++11 constant evaluation becomes necessary.
  std::vector<path_segment> path;

  // node_id refers to the caller-owned tree; field views may borrow its keys.
  // Missing fields may have no corresponding node.
  ::ryml::id_type node_id = ::ryml::NONE;

  // TODO: Accept retained parser source-location metadata when mapping;
  // standalone trees do not retain source positions.
  // TODO: Add constraint metadata and renderer customization when needed.
};

/** Collected mapping issues without ownership of their source tree.
 * Keep the tree and borrowed character storage alive and unchanged while
 * rendering diagnostics. Field paths may borrow metadata or source keys. */
struct diagnostics {
  // Mapping-policy snapshot; the deserialize transformation is not retained.
  omni::ryml::config<> config;

  // Validation and conversion issues in traversal order, including warnings.
  std::vector<issue> issues;

  // Owns RapidYAML syntax-error text returned by core::parse.
  // Empty when mapping an already parsed tree.
  std::string parse_error_message;
};

/** Diagnostics result produced by Config, preserving its cv/ref qualification. */
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
struct with_diagnostics {
  // Available only when mapping succeeds.
  compat::optional<T> value;

  // Retained issue records, or the result of transforming them for display.
  Diagnostics diagnostics;

  // TODO: Collect defaulted fields separately from warnings and errors.
  // TODO: Add and_then for generic expected-style sequencing; specify how to
  // retain diagnostics from both the input and the returned result.
  // TODO: Add or_else for generic expected-style recovery; specify how to
  // retain diagnostics when a value and issues are both present.
};

/** Borrowed path through active mapping states.
 * Copies retain parent links without allocating. Referenced parent paths must
 * remain alive and unchanged until child mappings finish. Issues materialize
 * their own segment arrays, retaining the field-name storage they borrow. */
struct mapping_path {
  // Root paths have depth zero; their parent and segment are unused.
  const mapping_path *parent;
  issue::path_segment segment;
  std::size_t depth;

  /** Copy the path's segments into root-to-leaf order for diagnostics. */
  std::vector<issue::path_segment> materialize() const;
};

/** Mapping configuration and traversal progress passed to customization points.
 * Config is borrowed; path links refer to active ancestor states. Copies do
 * not copy configuration or allocate path storage. Counters include issues
 * reported by preceding operations in the enclosing traversal. */
template <typename Config = omni::ryml::config<>>
struct mapping_state {
  const Config &config;
  mapping_path path;
  std::size_t error_count;
  std::size_t warning_count;

  // FIXME: [high] Preserve the builder's compile-time string-ownership policy
  // through the writing interface instead of erasing it into a runtime flag.
  bool borrow_strings;

  /** Whether an error or warning budget has been exceeded. */
  constexpr bool limit_exceeded() const {
    return this->config.error_policy.error_limit < this->error_count
      || (this->config.error_policy.warning_limit
        && *this->config.error_policy.warning_limit < this->warning_count);
  }

  /** Descend one path segment, preserving configuration and issue counts.
   * This state must remain alive while the returned child is used. */
  OMNI_CPP14_CONSTEXPR mapping_state child(issue::path_segment segment)
    const & {
    return {
      /*config=*/this->config,
      /*path=*/{&this->path, segment, this->path.depth + 1},
      /*error_count=*/this->error_count,
      /*warning_count=*/this->warning_count,
      /*borrow_strings=*/this->borrow_strings,
    };
  }

  // Child paths borrow the parent, so reject a temporary parent state.
  mapping_state child(issue::path_segment) const && = delete;
};

namespace cp {

/** Sentinel for an unregistered node-to-C++ customization. */
struct unavailable {};

/** Ad hoc discovery override for types inheriting standard-library containers.
 * Inheriting those containers is discouraged. Specialize to std::true_type
 * when a custom mapping needs the derived type's metadata: T itself is passed
 * to reflected_call instead of following its element or mapped type.
 *
 * The default follows the normal mapping traits. This override changes
 * discovery, not the representation selected by from_type/to_type.
 *
```cpp
struct tagged_values: std::vector<int> {
  int tag;
};

template <>
struct omni::ryml::cp::reflect_as_root<tagged_values>: std::true_type {};
``` */
template <typename T>
struct reflect_as_root:
    compat::negation<compat::disjunction<std::is_arithmetic<T>,
      traits::is_string<T>,
      detail::is_mapped_range<T>,
      traits::is_optional<T>>> {};

/** C++-to-node customization; the default maps reflected records.
 * A scalar wrapper can delegate conversion to its wrapped value:
 *
```cpp
struct counter { int value; };

template <>
struct omni::ryml::cp::from_type<counter> {
  template <typename Config>
  void operator()(const counter &value, ::ryml::NodeRef node,
    mapping_state<Config> context) const {
    omni::ryml::core::write_node(value.value, node, context);
  }
};
``` */
template <typename T, typename Enable = void>
struct from_type;

/** Node-to-C++ callable customization. Unregistered types use the default
 * compositor. Register either this struct or the variable template.
 *
```cpp
struct counter { int value; };

template <>
struct omni::ryml::cp::to_type_t<counter> {
  template <typename Config>
  auto operator()(::ryml::ConstNodeRef node, counter *into,
    mapping_state<Config> context) const
    -> omni::ryml::with_diagnostics<omni::compat::monostate> {
    return omni::ryml::core::read_node(node, &into->value, context);
  }
};
``` */
template <typename T, typename Enable = void>
struct to_type_t: unavailable {};

// Callable variable-template registration is available starting with C++14.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
/** Callable variable-template customization, available since C++14.
 * The selected const object is copied when mapping a node. Configured default
 * compositors can be registered directly:
 *
```cpp
struct endpoint { int port; };

template <>
const auto omni::ryml::cp::to_type<endpoint> =
  omni::ryml::reflected_scope::to_record.with_name_by_alias(
    omni::ryml::alias_dict(std::make_tuple("port", "service_port")));
``` */
template <typename T, typename Enable = void>
constexpr unavailable to_type{};
#endif

} // namespace cp

namespace core {

/** Dispatch one value through cp::from_type with its current mapping state. */
template <typename T, typename Config>
void write_node(const T &value, ::ryml::NodeRef node,
  mapping_state<Config> context);

/** Dispatch one node through the selected to_type callable with diagnostics. */
template <typename T, typename Config>
auto read_node(::ryml::ConstNodeRef node, T *into,
  mapping_state<Config> context)
  -> with_diagnostics<compat::monostate, diagnostics>;

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
``` */
inline compat::expected<::ryml::Tree, std::string> parse(std::string source,
  ::ryml::ParserOptions options = {});

/** Map tree nodes depth-first through the selected customization points.
 * @pre node must be valid. The caller retains its tree and character storage
 * while using borrowed values or rendering the returned diagnostics.
 * The diagnostics transformation in config is not applied by map_tree. */
template <typename Record, typename TransformDiagnostics>
with_diagnostics<Record> map_tree(const config<TransformDiagnostics> &options,
  omni::type_t<Record> record_type, ::ryml::ConstNodeRef node);

template <typename Record, typename TransformDiagnostics>
with_diagnostics<Record> map_tree(const config<TransformDiagnostics> &options,
  omni::type_t<Record> record_type, const ::ryml::Tree &tree);

// Mapping borrows the tree; a temporary cannot outlive the returned result.
template <typename Record, typename TransformDiagnostics>
void map_tree(const config<TransformDiagnostics> &, omni::type_t<Record>,
  const ::ryml::Tree &&) = delete;

/** Deserialize JSON/YAML text into the C++ type selected by record_type.
 * Return an optional value together with transformed diagnostics.
 */
OMNI_CONSTRAINED_TEMPLATE((typename Config, typename Record),
  (traits::is_specialization_of<config, Config>()))
auto deserialize(Config &&options, omni::type_t<Record> record_type,
  std::string source)
  -> with_diagnostics<Record, diagnostics_result_t<Config>>;

/** Build an owning RapidYAML tree from reflected records, supported scalars,
 * vectors, and optionals. Empty optionals produce null nodes; empty optional
 * sequence elements are omitted.
 * String data is copied into the tree's arena, so the tree can outlive the
 * source value. The returned tree can be modified or passed to RapidYAML's
 * emitters.
 */
template <typename T>
::ryml::Tree as_owning_tree(const T &value);

/** Build a RapidYAML tree borrowing the input's existing string storage.
 * The tree owns its nodes and generated scalar text. Borrowed storage must
 * remain alive and unchanged while the tree is used. Accepts const or mutable
 * lvalues; temporaries are rejected to prevent dangling views.
 */
template <typename T>
::ryml::Tree as_ref_tree(T &value);

// Also reject const temporaries, which could otherwise bind through Value&.
template <typename Value>
::ryml::Tree as_ref_tree(const Value &&) = delete;

} // namespace core

/// Map a caller-owned tree or subtree using the bound config.
template <typename Config = omni::ryml::config<>>
struct map_tree_t {
  static_assert(traits::is_specialization_of<omni::ryml::config, Config>(),
    "map_tree_t requires a specialization of omni::ryml::config");

  Config config;

  /** Transform data member `.config` inplace, in a single expression.
   * Invokes f immediately and returns a new binding with the resulting config,
   * leaving this binding unchanged. f :: Config -> NewConfig. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &, Config>::value))
  auto tdm_config(F &&f) const
    -> map_tree_t<compat::decay_t<compat::invoke_result_t<F &, Config>>> {
    return {compat::invoke(f, Config{this->config})};
  }

  template <typename Record>
  with_diagnostics<Record> operator()(omni::type_t<Record> record_type,
    const ::ryml::Tree &tree) const {
    return core::map_tree(this->config, record_type, tree);
  }

  template <typename Record>
  with_diagnostics<Record> operator()(omni::type_t<Record> record_type,
    ::ryml::ConstNodeRef node) const {
    return core::map_tree(this->config, record_type, node);
  }

  // Reject temporary trees: returned values and diagnostics may borrow storage.
  template <typename Record>
  void operator()(omni::type_t<Record>, const ::ryml::Tree &&) const = delete;
};

const map_tree_t<> map_tree{};

/** Parse JSON/YAML and return an optional value with transformed diagnostics.
 * The default config renders diagnostic text before releasing the source tree.
 * Custom transforms receive const Tree& and diagnostics while the tree lives.
 */
template <typename Config = omni::ryml::config<>>
struct deserialize_t {
  static_assert(traits::is_specialization_of<omni::ryml::config, Config>(),
    "deserialize_t requires a specialization of omni::ryml::config");

  Config config;

  /** Transform data member `.config` inplace, in a single expression.
   * Invokes f immediately and returns a new binding with the resulting config,
   * leaving this binding unchanged. f :: Config -> NewConfig. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &, Config>::value))
  auto tdm_config(F &&f) const
    -> deserialize_t<compat::decay_t<compat::invoke_result_t<F &, Config>>> {
    return {compat::invoke(f, Config{this->config})};
  }

  template <typename Record>
  auto operator()(omni::type_t<Record> record_type, std::string source) &
    -> decltype(core::deserialize(this->config, record_type, std::move(source))) {
    return core::deserialize(this->config, record_type, std::move(source));
  }

  template <typename Record>
  auto operator()(omni::type_t<Record> record_type, std::string source) const &
    -> decltype(core::deserialize(this->config, record_type, std::move(source))) {
    return core::deserialize(this->config, record_type, std::move(source));
  }

  template <typename Record>
  auto operator()(omni::type_t<Record> record_type, std::string source) &&
    -> decltype(core::deserialize(std::move(this->config), record_type,
      std::move(source))) {
    return core::deserialize(std::move(this->config), record_type,
      std::move(source));
  }

  // TODO: Add T *into overloads here and in core::deserialize to map into an
  // existing object. Define mutation on failure and the result contract;
  // do not require constructing and assigning a replacement object.
};

const deserialize_t<> deserialize{};

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
};

/**
 * Serialize reflected records, supported scalars, or vectors to YAML text.
 * Reads the value and its referenced storage through const references. Keep
 * them alive and unchanged until the call returns. Concurrent reads are safe;
 * synchronize any concurrent mutation or destruction of the input storage.
 *
```cpp
const auto yaml = as_yaml(value);
``` */
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
``` */
constexpr serialize_t as_json{
  /*format=*/::ryml::EMIT_JSON,
};

namespace cp {

// Record mapping is the fallback; matching specializations replace it.
template <typename T, typename Enable>
struct from_type: reflected_scope::from_record {};

template <typename T>
struct from_type<T,
  compat::enable_if_t<from_string_like::supports<T>()>>:
    from_string_like {};

template <typename T>
struct from_type<T,
  compat::enable_if_t<from_arithmetic::supports<T>()>>:
    from_arithmetic {};

template <typename T>
struct from_type<T,
  compat::enable_if_t<from_optional_like::supports<T>()>>:
    from_optional_like {};

template <typename T>
struct from_type<T,
  compat::enable_if_t<from_range_t::supports<T>()>>:
    from_range_t {};

template <typename T>
struct from_type<T,
  compat::enable_if_t<reflected_scope::from_enum::supports<T>()>>:
    reflected_scope::from_enum {};

// TODO: [high] Document and demonstrate the reflected-scope contract for
// from_type/to_type customizations. Metadata queries must depend on template
// arguments so instantiation is delayed until the reflected scope. A concrete
// `[](const Tree &) -> expected<my_type, error> { ... }` body is checked eagerly;
// metadata queries there can occur outside the reflected scope. Cover the
// dependent callable form in the guide before advertising callable CPs.

} // namespace cp

namespace detail {

// Built-ins stay separate from registrations so a user override never competes
// with a matching default partial specialization.
template <typename T, typename = void>
struct default_to_type:
    std::conditional<omni::is_reflected_record<T>::value,
      reflected_scope::to_record_t<>,
      cp::unavailable>::type {};

template <typename T>
struct default_to_type<T,
  compat::enable_if_t<to_string_like::supports<T>()>>:
    to_string_like {};

template <typename T>
struct default_to_type<T,
  compat::enable_if_t<to_arithmetic::supports<T>()>>:
    to_arithmetic {};

template <typename T>
struct default_to_type<T,
  compat::enable_if_t<to_optional_like::supports<T>()>>:
    to_optional_like {};

template <typename T>
struct default_to_type<T,
  compat::enable_if_t<to_range_t::supports<T>()>>:
    to_range_t {};

template <typename T>
struct default_to_type<T,
  compat::enable_if_t<reflected_scope::to_enum_t<>::supports<T>()>>:
    reflected_scope::to_enum_t<> {};

template <typename T>
struct to_type_registration {
  static constexpr bool has_struct =
    !std::is_base_of<cp::unavailable, cp::to_type_t<T>>::value;

// C++11 registers structs; C++14 also allows callable variable templates.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
  static constexpr bool has_variable = !std::is_same<cp::unavailable,
    compat::remove_cvref_t<decltype(cp::to_type<T>)>>::value;
#else
  static constexpr bool has_variable = false;
#endif
};

template <typename T,
  bool Struct = to_type_registration<T>::has_struct,
  bool Variable = to_type_registration<T>::has_variable>
struct to_type_selection {
  using type = cp::to_type_t<T>;
  static constexpr type get() { return {}; }
};

// Read the registered object itself so its configured state is preserved.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <typename T>
struct to_type_selection<T, false, true> {
  using type = compat::remove_cvref_t<decltype(cp::to_type<T>)>;
  static constexpr type get() { return cp::to_type<T>; }
};
#endif

template <typename T>
struct to_type_selection<T, false, false> {
  using type = default_to_type<T>;
  static constexpr type get() { return {}; }
};

} // namespace detail

/** Return the registered node-to-C++ callable by value, or the default
 * compositor when neither customization is registered. Selected callables
 * must be trivially copyable and copy-constructible. Registration conflicts
 * and missing mappings are diagnosed here, before invoking the mapper. */
template <typename T>
constexpr auto to_type(omni::type_t<T>)
  -> typename detail::to_type_selection<T>::type {
  using mapper = typename detail::to_type_selection<T>::type;

  static_assert(!(detail::to_type_registration<T>::has_struct
      && detail::to_type_registration<T>::has_variable),
    "Conflicting node mappings: specialize either cp::to_type_t or "
    "cp::to_type, not both");

  static_assert(!std::is_base_of<cp::unavailable, mapper>::value,
    "No node mapping: specialize cp::to_type_t or cp::to_type");

  static_assert(compat::conjunction<std::is_trivially_copyable<mapper>,
      std::is_copy_constructible<mapper>>::value,
    "Node mapping callables must be trivially copyable and copy-constructible");

  return detail::to_type_selection<T>::get();
}

} // namespace ryml
} // namespace omni

// Inline and template definitions depend on the complete public interface
// above. Including them here keeps the detail header private to this header.
#include <omnirefl/serialization/impl/ryml.hpp>
#include <omnirefl/serialization/ryml/impl/mapping.hpp>
#include <omnirefl/serialization/ryml/impl/mapping/enum.hpp>
#include <omnirefl/serialization/ryml/impl/mapping/record.hpp>
#include <omnirefl/serialization/ryml/impl/mapping/sequence.hpp>

// Built-in string operations must satisfy the same contract as custom strings.
static_assert(omni::traits::is_string<std::string>::value,
  "std::string must provide the string mapping operations");
static_assert(omni::traits::is_string<c4::csubstr>::value,
  "c4::csubstr must provide the string mapping operations");
#if defined(__cpp_lib_string_view)
static_assert(omni::traits::is_string<std::string_view>::value,
  "std::string_view must provide the string mapping operations");
#endif

#if defined(OMNI_TOOL_RUN)
// TU-wide: c4 views use string writers; compatibility wrappers expose their
// contents through value_type/error_type without needing wrapper metadata.
// Ad hoc: skip requests accept types, so int fills the required template
// arguments. Matching selects the primary templates, so every specialization
// is skipped.
OMNI_SKIP_METADATA(c4::csubstr,
  omni::compat::optional<int>,
  omni::compat::expected<int, int>);
#endif
