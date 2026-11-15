#pragma once

#if !defined(OMNI_TOOL_RUN) \
  && !defined(OMNI_INCLUDED_GENERATED_REFLECTION_HEADER)
#  error \
    "Include the generated reflection header before serialization. " \
    "With CMake, call omni_reflected_target(<target>)."
#endif

/** JSON/YAML serialization and deserialization using RapidYAML.
 * deserialize reads text into C++ values and returns diagnostics; as_json and
 * as_yaml write those values back. Default mappings cover reflected records
 * and enums, arithmetic values, strings, optionals, sequences, and string-keyed
 * containers. Customization points adapt other types and representations.
 *
 * This header supplies the complete public interface, including configuration,
 * read/write states, and default mappers. core::parse and map_by_schema also
 * support composing parsing and schema-driven mapping explicitly.
 *
 * Compatible with C++11 and later; examples use C++20 syntax for brevity.
 * Guide: extras/extensions/serialization/guide/main.cpp.
 * C++11 examples: extras/extensions/serialization/tests/main.cpp.
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

// FIXME: [low] Freestanding: public diagnostics and text APIs require hosted
// owning strings and vectors; provide an alternative representation.

#include <omnirefl/reflected_scope.hpp>

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/traits.hpp>
#include <omnirefl/serialization/ryml/state.hpp>
#include <omnirefl/serialization/ryml/mapping.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <string>
#include <type_traits>
#include <utility>

namespace omni {
namespace traits {
namespace cp {

// c4 views use the same operation customization points as user strings.
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
      traits::is_string<T>,
      detail::is_mapped_range<T>,
      traits::is_optional<T>>> {};

/** C++11 C++-to-node customization; unregistered types use a default mapper.
 * A scalar wrapper can delegate conversion to its wrapped value:
 *
```cpp
struct counter { int value; };

template <>
struct omni::ryml::cp::from_type_t<counter> {
  OMNI_CONSTRAINED_TEMPLATE((typename State),
    (omni::ryml::is_write_state<State>::value))
  void operator()(const counter &value, ::ryml::NodeRef node,
    State context) const {
    omni::ryml::core::write_node(value.value, node, context);
  }
};
``` */
template <typename T, typename Enable = void>
struct from_type_t: traits::cp::unavailable {};

// C++14 adds variable-template registration for serialization callables.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
/** C++-to-node callable customization. Register either this variable template
 * or from_type_t for a given type; the dispatcher selects a default otherwise.
 */
template <typename T, typename Enable = void>
constexpr traits::cp::unavailable from_type{};
#endif

/** Node-to-C++ callable customization. Unregistered types use the default
 * compositor. Register either this struct or the variable template.
 *
```cpp
struct counter { int value; };

template <>
struct omni::ryml::cp::to_type_t<counter> {
  OMNI_CONSTRAINED_TEMPLATE((typename State),
    (omni::ryml::is_read_state<State>::value))
  auto operator()(::ryml::ConstNodeRef node, counter *into,
    State context) const
    -> omni::ryml::with_diagnostics_t<omni::compat::monostate> {
    return omni::ryml::core::read_node(node, &into->value, context);
  }
};
``` */
template <typename T, typename Enable = void>
struct to_type_t: traits::cp::unavailable {};

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
constexpr traits::cp::unavailable to_type{};
#endif

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

/** Dispatch one value through its serialization customization or default. */
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (is_write_state<State>::value))
void write_node(const T &value, ::ryml::NodeRef node, State context);

/** Dispatch one node through the selected to_type callable with diagnostics. */
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (is_read_state<State>::value))
auto read_node(::ryml::ConstNodeRef node, T *into, State context)
  -> with_diagnostics_t<compat::monostate, diagnostics>;

/** Map document nodes into a C++ value, traversing its schema over the
 * document through the selected customization points.
 * @pre node must be valid. The caller retains its tree and character storage
 * while using borrowed values or rendering the returned diagnostics.
 * The diagnostics transformation in config is not applied by map_by_schema. */
OMNI_CONSTRAINED_TEMPLATE((typename Config, typename Record),
  (is_config<Config>::value))
auto map_by_schema(const Config &options, omni::type_t<Record> record_type,
  ::ryml::ConstNodeRef node) -> with_diagnostics_t<Record>;

OMNI_CONSTRAINED_TEMPLATE((typename Config, typename Record),
  (is_config<Config>::value))
auto map_by_schema(const Config &options, omni::type_t<Record> record_type,
  const ::ryml::Tree &tree) -> with_diagnostics_t<Record>;

// Mapping borrows the tree; a temporary cannot outlive the returned result.
OMNI_CONSTRAINED_TEMPLATE((typename Config, typename Record),
  (is_config<Config>::value))
void map_by_schema(const Config &, omni::type_t<Record>,
  const ::ryml::Tree &&) = delete;

/** Deserialize JSON/YAML text into the C++ type selected by record_type.
 * Return an optional value together with transformed diagnostics.
 */
OMNI_CONSTRAINED_TEMPLATE((typename Config, typename Record),
  (is_config<Config>::value))
auto deserialize(Config &&options, omni::type_t<Record> record_type,
  std::string source)
  -> with_diagnostics_t<Record, diagnostics_result_t<Config>>;

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

  template <typename Record>
  with_diagnostics_t<Record> operator()(omni::type_t<Record> record_type,
    const ::ryml::Tree &tree) const {
    return core::map_by_schema(this->config, record_type, tree);
  }

  template <typename Record>
  with_diagnostics_t<Record> operator()(omni::type_t<Record> record_type,
    ::ryml::ConstNodeRef node) const {
    return core::map_by_schema(this->config, record_type, node);
  }

  // Reject temporary trees: returned values and diagnostics may borrow storage.
  template <typename Record>
  void operator()(omni::type_t<Record>, const ::ryml::Tree &&) const = delete;
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

  template <typename Record>
  auto operator()(omni::type_t<Record> record_type, std::string source) &
    -> decltype(core::deserialize(this->config, record_type,
      std::move(source))) {
    return core::deserialize(this->config, record_type, std::move(source));
  }

  template <typename Record>
  auto operator()(omni::type_t<Record> record_type, std::string source) const &
    -> decltype(core::deserialize(this->config, record_type,
      std::move(source))) {
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

namespace detail {

// Declared for the public accessor's dependent return type; registration
// selection is defined in the implementation header.
template <typename T>
struct from_type_selection_t;

// Declared for the public accessor's dependent return type; registration
// selection is defined in the implementation header.
template <typename T>
struct to_type_selection_t;

} // namespace detail

/** Return the registered C++-to-node callable by value, or the default
 * compositor when neither customization is registered. Selected callables
 * must be trivially copyable and copy-constructible. Registration conflicts
 * and missing mappings are diagnosed here, before invoking the mapper. */
template <typename T>
constexpr auto from_type(omni::type_t<T>)
  -> typename detail::from_type_selection_t<T>::type;

/** Return the registered node-to-C++ callable by value, or the default
 * compositor when neither customization is registered. Selected callables
 * must be trivially copyable and copy-constructible. Registration conflicts
 * and missing mappings are diagnosed here, before invoking the mapper. */
template <typename T>
constexpr auto to_type(omni::type_t<T>)
  -> typename detail::to_type_selection_t<T>::type;

} // namespace ryml
} // namespace omni

// Entry-point definitions depend on the complete public interface above.
#include <omnirefl/serialization/ryml/impl.hpp>

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
