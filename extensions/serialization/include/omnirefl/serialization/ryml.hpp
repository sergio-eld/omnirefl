#pragma once

/** JSON/YAML serialization and deserialization using RapidYAML.
 * deserialize reads text into C++ values and returns diagnostics; as_json and
 * as_yaml write those values back. Default mappings cover reflected records
 * and enums, arithmetic values, strings, optionals, sequences, and string-keyed
 * containers. Customization points adapt other types and representations.
 *
 * This header supplies the complete public interface, including configuration,
 * read/write states, and default type mappings. core::parse and map_by_schema
 * also support composing parsing and schema-driven mapping explicitly.
 *
 * Compatible with C++11 and later; examples use C++20 syntax for brevity.
 * Guide: extras/extensions/serialization/guide/main.cpp.
 * C++11 examples: extras/extensions/serialization/tests/main.cpp.
 */

// FIXME: [low] Freestanding: public diagnostics and text APIs require hosted
// owning strings and vectors; provide an alternative representation.

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/traits.hpp>
#include <omnirefl/reflected_scope.hpp>
#include <omnirefl/serialization/ryml/state.hpp>
#include <omnirefl/serialization/ryml/type_mapping.hpp>
#include <omnirefl/traits.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <string>
#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {
namespace cp {

/** Ad hoc discovery override for types inheriting standard-library containers.
 * Inheriting those containers is discouraged. Specialize to std::true_type
 * when a custom mapping needs the derived type's metadata: T itself is passed
 * to reflected_call instead of following its element or mapped type.
 *
 * The default follows the normal mapping traits. This override changes
 * discovery, not the representation selected by the mapping customization.
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
      traits::is_string_like<T>,
      detail::is_mapped_range<T>,
      traits::is_optional<T>>> {};

} // namespace cp

namespace core {

/**
 * Non-throwing adapter for RapidYAML parsing errors. Parses text into an
 * owning tree and returns syntax errors through expected.
 * ParserOptions are passed directly to ryml. Location tracking, if enabled,
 * ends with this call; the returned tree does not retain the parser.
 *
```cpp
// Supply a reflected person, config, and an owning std::string source.
auto parsed = core::parse(std::move(source));
if (parsed) {
  auto result = core::map_by_schema(options, omni::type_t<person>{}, *parsed);
  // Use result while parsed keeps its tree and character storage alive.
}
// Syntax errors use expected; mapping returns optional person and diagnostics.
``` */
inline compat::expected<::ryml::Tree, std::string> parse(std::string source,
  ::ryml::ParserOptions options = {});

// REFACTORME: [high] Review responsibility for diagnostic accumulation and
// limit checks between this dispatcher and the node mappers.
/** Dispatch one node through the selected to_type callable with diagnostics. */
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (is_read_state<State>::value))
auto read_node(::ryml::ConstNodeRef node, T *into, State context)
  -> with_diagnostics_t<compat::monostate, diagnostics>;

// REFACTORME: [high] This only forwards to mapping_from_type(...)(...).
// Reassess whether a separate writer dispatcher is needed.
/** Dispatch one value through its serialization customization or default. */
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (is_write_state<State>::value))
void write_node(const T &value, ::ryml::NodeRef node, State context);

/** (Ad hoc: see detail) Map Boolean, integer, or real JSON/YAML nodes
 * into corresponding fundamental C++ types.
 *
 * @details These routes sanitize compound destination types before
 * reflected_call. Arithmetic values bypass reflection. Registered readers
 * take precedence; otherwise to_boolean, to_integer, and to_real perform
 * the format checks and conversion.
 * @pre node must be valid and into must be non-null. */
struct map_to_fundamental_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_arithmetic<T>::value && !std::is_const<T>::value
      && is_read_state<State>::value))
  auto operator()(::ryml::ConstNodeRef node, T *into, State context) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;
};

constexpr map_to_fundamental_t map_to_fundamental{};

/** (Ad hoc: see detail) Map JSON/YAML nodes into string-like C++ types.
 *
 * @details These routes sanitize compound destination types before
 * reflected_call. String registrations and adapters require no metadata,
 * including for instrumentation-disabled types. Registered readers take
 * precedence over to_string_like without making the string a reflection target.
 * @pre node must be valid and into must be non-null. */
struct map_to_string_like_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (traits::is_string_like<T>::value && !std::is_const<T>::value
      && is_read_state<State>::value))
  auto operator()(::ryml::ConstNodeRef node, T *into, State context) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;
};

constexpr map_to_string_like_t map_to_string_like{};

/** (Ad hoc: see detail) Map JSON/YAML nodes into C++ types requiring
 * reflected scope.
 *
 * @details reflected_call needs a metadata-bearing type, not an arbitrary
 * compound destination. Range and optional wrappers are unwrapped to obtain
 * that reflection target while mapping retains the original destination.
 * @pre node must be valid and into must be non-null. */
struct map_to_reflected_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_object<T>::value && !std::is_const<T>::value
      && is_read_state<State>::value))
  auto operator()(::ryml::ConstNodeRef node, T *into, State context) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;
};

constexpr map_to_reflected_t map_to_reflected{};

/** (Ad hoc: see detail) Map JSON/YAML arrays or objects into C++ ranges.
 *
 * @details reflected_call cannot use the compound range as its reflection
 * target. Discovery follows element or mapped-value types before establishing
 * scope, preserving the original range as the destination. Fundamental and
 * string-like elements require no reflected scope, including empty ranges.
 * @pre node must be valid and into must be non-null. */
struct map_to_sequence_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (omni::ryml::detail::is_mapped_range<T>::value && !std::is_const<T>::value
      && is_read_state<State>::value))
  auto operator()(::ryml::ConstNodeRef node, T *into, State context) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;
};

constexpr map_to_sequence_t map_to_sequence{};

/** (Ad hoc: see detail) Map JSON/YAML nodes into optional-like C++ types.
 *
 * @details reflected_call requires sanitizing this compound destination to
 * its contained type. Discovery establishes scope only when needed and keeps
 * the optional as the mapping destination, including when it is empty.
 * @pre node must be valid and into must be non-null. */
struct map_to_optional_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (traits::is_optional<T>::value && !std::is_const<T>::value
      && is_read_state<State>::value))
  auto operator()(::ryml::ConstNodeRef node, T *into, State context) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;
};

constexpr map_to_optional_t map_to_optional{};

/** Map document nodes into *into, traversing its schema over the document.
 * Successfully mapped ranges replace their existing contents. Mapping errors
 * leave the destination state unspecified and may produce partial updates.
 * value holds monostate only when mapping succeeds.
 * @pre into must be non-null and node must be valid. The caller retains the
 * tree and character storage while using borrowed values or diagnostics.
 * The diagnostics transformation in config is not applied. */
OMNI_CONSTRAINED_TEMPLATE((typename Config, typename T),
  (is_config<Config>::value && std::is_object<T>::value
    && !std::is_const<T>::value))
auto map_by_schema(const Config &options, T *into, ::ryml::ConstNodeRef node)
  -> with_diagnostics_t<compat::monostate> {
  const auto map = typename traits::select<
    traits::case_<cp::reflect_as_root<T>::value, map_to_reflected_t>,
    traits::case_<traits::is_string_like<T>::value, map_to_string_like_t>,
    traits::case_<std::is_arithmetic<T>::value, map_to_fundamental_t>,
    traits::case_<traits::is_optional<T>::value, map_to_optional_t>,
    traits::case_<omni::ryml::detail::is_mapped_range<T>::value,
      map_to_sequence_t>,
    map_to_reflected_t>::type{};

  return map(node, into, read_state_t<Config>{
    /*config=*/options,
    /*path=*/{},
    /*error_count=*/0,
    /*warning_count=*/0,
  });
}

/** Map document nodes into a C++ value, traversing its schema over the
 * document through the selected customization points.
 * @pre node must be valid. The caller retains its tree and character storage
 * while using borrowed values or rendering the returned diagnostics.
 * The diagnostics transformation in config is not applied by map_by_schema. */
OMNI_CONSTRAINED_TEMPLATE((typename Config, typename Record),
  (is_config<Config>::value && std::is_object<Record>::value
    && !std::is_const<Record>::value))
auto map_by_schema(const Config &options, omni::type_t<Record>,
  ::ryml::ConstNodeRef node) -> with_diagnostics_t<Record> {
  auto value = compat::make_optional<Record>();
  auto mapped = map_by_schema(options, &*value, node);
  return {
    /*value=*/mapped.value ? std::move(value) : compat::nullopt,
    /*diagnostics=*/std::move(mapped.diagnostics),
  };
}

/** (QoL) Map a whole tree without explicitly extracting its root node. */
OMNI_CONSTRAINED_TEMPLATE((typename Config, typename Record),
  (is_config<Config>::value && std::is_object<Record>::value
    && !std::is_const<Record>::value))
auto map_by_schema(const Config &options, omni::type_t<Record> record_type,
  const ::ryml::Tree &tree) -> with_diagnostics_t<Record>;

// Deleted: mapped values and diagnostics may borrow the tree's storage.
OMNI_CONSTRAINED_TEMPLATE((typename Config, typename Record),
  (is_config<Config>::value && std::is_object<Record>::value
    && !std::is_const<Record>::value))
void map_by_schema(const Config &, omni::type_t<Record>,
  const ::ryml::Tree &&) = delete;

/** (QoL) Map a whole tree without explicitly extracting its root node. */
OMNI_CONSTRAINED_TEMPLATE((typename Config, typename T),
  (is_config<Config>::value && std::is_object<T>::value
    && !std::is_const<T>::value))
auto map_by_schema(const Config &options, T *into, const ::ryml::Tree &tree)
  -> with_diagnostics_t<compat::monostate>;

// Deleted: mapped values and diagnostics may borrow the tree's storage.
OMNI_CONSTRAINED_TEMPLATE((typename Config, typename T),
  (is_config<Config>::value && std::is_object<T>::value
    && !std::is_const<T>::value))
void map_by_schema(const Config &, T *, const ::ryml::Tree &&) = delete;

/** Deserialize JSON/YAML text into the C++ type selected by record_type.
 * Return an optional value together with transformed diagnostics.
 */
OMNI_CONSTRAINED_TEMPLATE((typename Config, typename Record),
  (is_config<Config>::value && std::is_object<Record>::value
    && !std::is_const<Record>::value))
auto deserialize(Config &&options, omni::type_t<Record> record_type,
  std::string source)
  -> with_diagnostics_t<Record, diagnostics_result_t<Config>>;

/** Deserialize JSON/YAML text into *into, returning transformed diagnostics.
 * Successfully mapped ranges replace their existing contents. Syntax errors
 * leave *into unchanged; on mapping failure, its state is unspecified and may
 * contain partial updates.
 * value holds monostate only when mapping succeeds.
 * @pre into must be non-null. */
OMNI_CONSTRAINED_TEMPLATE((typename Config, typename T),
  (is_config<Config>::value && std::is_object<T>::value
    && !std::is_const<T>::value))
auto deserialize(Config &&options, T *into, std::string source)
  -> with_diagnostics_t<compat::monostate, diagnostics_result_t<Config>>;

// REFACTORME: [high] Expose type-switch routing for as_owning_tree and
// as_ref_tree, as map_by_schema does. A clean way to share the routing between
// these builders and mapping, without duplication, remains to be designed.
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

// Deleted: a reference tree must not borrow from a temporary source.
template <typename Value>
::ryml::Tree as_ref_tree(const Value &&) = delete;

} // namespace core

/** Schema-driven mapping of a caller-owned tree or subtree with bound config.
 */
template <typename Config = omni::ryml::config_t<>>
struct map_by_schema_t {
  static_assert(is_config<Config>::value,
    "map_by_schema_t requires a specialization of omni::ryml::config_t");

  Config config;

  /** (QoL) Transform data member `.config` inplace, in a single expression.
   * Invokes f immediately and returns a new binding with the resulting config,
   * preserving this binding. f :: const Config& -> NewConfig. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, const Config &>::value))
  constexpr auto tdm_config(F &&f) const &
    -> map_by_schema_t<compat::decay_t<
      compat::invoke_result_t<F &&, const Config &>>> {
    return {/*config=*/compat::invoke(std::forward<F>(f), this->config)};
  }

  /** (QoL) Transform data member `.config`, consuming this binding. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, Config &&>::value))
  // C++11 constexpr member functions are implicitly const, preventing moves.
  OMNI_CPP14_CONSTEXPR auto tdm_config(F &&f) &&
    -> map_by_schema_t<
      compat::decay_t<compat::invoke_result_t<F &&, Config &&>>> {
    return {
      /*config=*/compat::invoke(std::forward<F>(f), std::move(this->config)),
    };
  }

  OMNI_CONSTRAINED_TEMPLATE((typename Record),
    (std::is_object<Record>::value && !std::is_const<Record>::value))
  with_diagnostics_t<Record> operator()(omni::type_t<Record> record_type,
    const ::ryml::Tree &tree) const {
    return core::map_by_schema(this->config, record_type, tree);
  }

  OMNI_CONSTRAINED_TEMPLATE((typename Record),
    (std::is_object<Record>::value && !std::is_const<Record>::value))
  with_diagnostics_t<Record> operator()(omni::type_t<Record> record_type,
    ::ryml::ConstNodeRef node) const {
    return core::map_by_schema(this->config, record_type, node);
  }

  OMNI_CONSTRAINED_TEMPLATE((typename T),
    (std::is_object<T>::value && !std::is_const<T>::value))
  auto operator()(T *into, const ::ryml::Tree &tree) const
    -> decltype(core::map_by_schema(this->config, into, tree)) {
    return core::map_by_schema(this->config, into, tree);
  }

  OMNI_CONSTRAINED_TEMPLATE((typename T),
    (std::is_object<T>::value && !std::is_const<T>::value))
  auto operator()(T *into, ::ryml::ConstNodeRef node) const
    -> decltype(core::map_by_schema(this->config, into, node)) {
    return core::map_by_schema(this->config, into, node);
  }

  // Deleted: mapped values and diagnostics may borrow the tree's storage.
  OMNI_CONSTRAINED_TEMPLATE((typename Record),
    (std::is_object<Record>::value && !std::is_const<Record>::value))
  void operator()(omni::type_t<Record>, const ::ryml::Tree &&) const = delete;

  // Deleted: mapped values and diagnostics may borrow the tree's storage.
  OMNI_CONSTRAINED_TEMPLATE((typename T),
    (std::is_object<T>::value && !std::is_const<T>::value))
  void operator()(T *, const ::ryml::Tree &&) const = delete;
};

const map_by_schema_t<> map_by_schema{};

/** (QoL) Partial application of core::deserialize with inspectable config.
 * Provides the global default deserialize; tdm_config transforms its bound
 * config in a single expression, returning a new binding.
 * Its call operators preserve the core template's argument constraints through
 * the C++11-compatible interface.
 */
template <typename Config = omni::ryml::config_t<>>
struct deserialize_t {
  static_assert(is_config<Config>::value,
    "deserialize_t requires a specialization of omni::ryml::config_t");

  Config config;

  /** (QoL) Transform data member `.config` inplace, in a single expression.
   * Invokes f immediately and returns a new binding with the resulting config,
   * preserving this binding. f :: const Config& -> NewConfig. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, const Config &>::value))
  constexpr auto tdm_config(F &&f) const &
    -> deserialize_t<compat::decay_t<
      compat::invoke_result_t<F &&, const Config &>>> {
    return {/*config=*/compat::invoke(std::forward<F>(f), this->config)};
  }

  /** (QoL) Transform data member `.config`, consuming this binding. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, Config &&>::value))
  // C++11 constexpr member functions are implicitly const, preventing moves.
  OMNI_CPP14_CONSTEXPR auto tdm_config(F &&f) &&
    -> deserialize_t<compat::decay_t<
      compat::invoke_result_t<F &&, Config &&>>> {
    return {
      /*config=*/compat::invoke(std::forward<F>(f), std::move(this->config)),
    };
  }

  OMNI_CONSTRAINED_TEMPLATE((typename Record),
    (std::is_object<Record>::value && !std::is_const<Record>::value))
  auto operator()(omni::type_t<Record> record_type, std::string source) &
    -> decltype(core::deserialize(this->config, record_type,
      std::move(source))) {
    return core::deserialize(this->config, record_type, std::move(source));
  }

  OMNI_CONSTRAINED_TEMPLATE((typename Record),
    (std::is_object<Record>::value && !std::is_const<Record>::value))
  auto operator()(omni::type_t<Record> record_type, std::string source) const &
    -> decltype(core::deserialize(this->config, record_type,
      std::move(source))) {
    return core::deserialize(this->config, record_type, std::move(source));
  }

  OMNI_CONSTRAINED_TEMPLATE((typename Record),
    (std::is_object<Record>::value && !std::is_const<Record>::value))
  auto operator()(omni::type_t<Record> record_type, std::string source) &&
    -> decltype(core::deserialize(std::move(this->config), record_type,
      std::move(source))) {
    return core::deserialize(std::move(this->config), record_type,
      std::move(source));
  }

  OMNI_CONSTRAINED_TEMPLATE((typename T),
    (std::is_object<T>::value && !std::is_const<T>::value))
  auto operator()(T *into, std::string source) &
    -> decltype(core::deserialize(this->config, into, std::move(source))) {
    return core::deserialize(this->config, into, std::move(source));
  }

  OMNI_CONSTRAINED_TEMPLATE((typename T),
    (std::is_object<T>::value && !std::is_const<T>::value))
  auto operator()(T *into, std::string source) const &
    -> decltype(core::deserialize(this->config, into, std::move(source))) {
    return core::deserialize(this->config, into, std::move(source));
  }

  OMNI_CONSTRAINED_TEMPLATE((typename T),
    (std::is_object<T>::value && !std::is_const<T>::value))
  auto operator()(T *into, std::string source) &&
    -> decltype(core::deserialize(std::move(this->config), into,
      std::move(source))) {
    return core::deserialize(std::move(this->config), into, std::move(source));
  }
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
 * Keep their storage alive and unchanged until the call returns, including
 * mutable data members and storage exposed by custom iterators.
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

} // namespace ryml
} // namespace omni

// Entry-point definitions depend on the complete public interface above.
#include <omnirefl/serialization/ryml/impl.hpp>

// Built-in string operations must satisfy the same contract as custom strings.
static_assert(omni::traits::is_string_like<std::string>::value,
  "std::string must provide the string mapping operations");
static_assert(omni::traits::is_string_like<c4::csubstr>::value,
  "c4::csubstr must provide the string mapping operations");
#if defined(__cpp_lib_string_view)
static_assert(omni::traits::is_string_like<std::string_view>::value,
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

// TODO: [medium] Review mapper copy-constructible requirements so
// configured callables can retain move-only state where mapping permits it.

// TODO: [high] Define how deserialize can return values borrowing tree storage.
// Such values need an external owner; map_by_schema lets the caller retain
// its tree.

// TODO: Move the omni::json mapping contracts to a shared header if
// another JSON parser is adopted.

// TODO: Consider global default-config customization.

// TODO: Remove core::parse when RapidYAML provides native non-throwing
// parsing with a result-based error interface.

// TODO: [low] Consider warning thresholds per policy in json::error_policy.

// TODO: [high] Consider a configurable mapping-depth limit for recursive
// deserialization to prevent stack exhaustion. It should also apply when
// map_by_schema receives an existing tree.

// TODO: [medium] Consider a config_t input-size limit checked by deserialize
// before parsing or allocating a tree. Source text is already buffered in a
// std::string; callers still need to bound reads before supplying it.

// TODO: [medium] Consider compile-time detection of models permitting unbounded
// nesting, such as a record containing a vector of itself. This can identify
// recursive schemas but does not replace a runtime mapping-depth limit.

// TODO: [medium] Define default variant mapping before supporting std::variant.
// Consider a discriminator selecting an alternative, its location and metadata,
// and whether untagged matching is allowed. Trying every alternative may be
// ambiguous and repeat expensive work; JSON Schema oneOf does not select one.

// TODO: [low] Explore SAX-driven validation and assignment as an alternative to
// parsing a whole tree before mapping. Early rejection could avoid retaining
// unused subtrees. Preserve mapping policies and diagnostics, check missing
// fields when objects close, and resolve YAML aliases and diagnostic lifetimes.

// TODO: [low] Consider exposing core::parse's existing ryml::ParserOptions
// through config_t if needed, without duplicating the backend's option fields.

// REFACTORME: [medium] Consider a dedicated enumerator type with named value
// and name fields instead of std::pair/compat::pair.
