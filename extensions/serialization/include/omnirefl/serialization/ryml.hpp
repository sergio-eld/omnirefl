#pragma once

/** JSON/YAML serialization and deserialization using RapidYAML.
 * deserialize reads text into C++ values and returns diagnostics; as_json and
 * as_yaml write those values back. Default mappings cover reflected records
 * and enums, arithmetic values, strings, optionals, sequences, and string-keyed
 * containers. Customization points adapt other types and representations.
 *
 * This header supplies the complete public interface, including configuration,
 * read/write states, and default type mappings. core::parse and map_by_schema
 * support composing parsing and schema-driven mapping explicitly. Entry points
 * are callable objects; their config members can be set at construction or
 * transformed through tdm_config.
 *
 * Compatible with C++11 and later; examples use C++20 syntax for brevity.
 * Guide: extras/extensions/serialization/guide/main.cpp.
 * C++11 examples: extras/extensions/serialization/tests/serialization.cpp.
 */

// FIXME: [low] Freestanding: public diagnostics and text APIs require hosted
// owning strings and vectors; provide an alternative representation.

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/traits.hpp>
#include <omnirefl/reflected_scope.hpp>
#include <omnirefl/serialization/ryml/core.hpp>
#include <omnirefl/serialization/ryml/state.hpp>
#include <omnirefl/serialization/ryml/type_mapping.hpp>
#include <omnirefl/traits.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <cassert>
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

namespace detail {

template <typename T>
using map_by_schema_dispatch_t = typename traits::select<
  traits::case_<cp::reflect_as_root<T>::value, core::map_to_reflected_t<>>,
  traits::case_<traits::is_string_like<T>::value, core::map_to_string_like_t<>>,
  traits::case_<std::is_arithmetic<T>::value, core::map_to_fundamental_t<>>,
  traits::case_<traits::is_optional<T>::value, core::map_to_optional_t<>>,
  traits::case_<is_mapped_range<T>::value, core::map_to_sequence_t<>>,
  core::map_to_reflected_t<>>::type;

} // namespace detail

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

  /** Map a node into T by traversing its C++ schema over the node.
   * Value mappings construct T directly; pointer-only mappings require T to
   * be default-constructible. Returns structured diagnostics without applying
   * config.transform_diagnostics.
   * @pre node must be valid. Keep its tree and character storage alive while
   * using borrowed values or diagnostics. */
  OMNI_CONSTRAINED_TEMPLATE((typename T),
    (std::is_object<T>::value && !std::is_const<T>::value))
  auto operator()(omni::type_t<T> type, ::ryml::ConstNodeRef node) const
    -> with_diagnostics_t<T> {
    // Borrowing config must not change the state type accepted by mappings.
    return detail::map_by_schema_dispatch_t<T>{}(
      read_state_t<compat::remove_cvref_t<Config>>{
        /*config=*/this->config,
        /*error_count=*/0,
        /*warning_count=*/0,
      },

      node,
      type);
  }

  /** (QoL) Map a whole tree into T through its root node.
   * Returns structured diagnostics; config.transform_diagnostics is not
   * applied. Keep the tree and its character storage alive while using
   * borrowed values or diagnostics. */
  OMNI_CONSTRAINED_TEMPLATE((typename T),
    (std::is_object<T>::value && !std::is_const<T>::value))
  auto operator()(omni::type_t<T> type, const ::ryml::Tree &tree) const
    -> with_diagnostics_t<T> {
    return (*this)(type, tree.crootref());
  }

  // Deleted: mapped values and diagnostics may borrow the tree's storage.
  OMNI_CONSTRAINED_TEMPLATE((typename T),
    (std::is_object<T>::value && !std::is_const<T>::value))
  void operator()(omni::type_t<T>, const ::ryml::Tree &&) const = delete;

  /** Map a node into *into by traversing its C++ schema over the node.
   * Ranges replace existing contents. Mapping errors leave the destination
   * state unspecified and may produce partial updates. Success holds
   * monostate.
   * Returns structured diagnostics; config.transform_diagnostics is not
   * applied.
   * @pre node must be valid and into must be non-null. Keep the tree and
   * character storage alive while using borrowed values or diagnostics. */
  OMNI_CONSTRAINED_TEMPLATE((typename T),
    (std::is_object<T>::value && !std::is_const<T>::value))
  auto operator()(T *into, ::ryml::ConstNodeRef node) const
    -> with_diagnostics_t<compat::monostate> {
    return detail::map_by_schema_dispatch_t<T>{}(
      read_state_t<compat::remove_cvref_t<Config>>{
        /*config=*/this->config,
        /*error_count=*/0,
        /*warning_count=*/0,
      },

      node,
      into);
  }

  /** (QoL) Map a whole tree into *into through its root node.
   * Ranges replace existing contents; mapping errors leave destination state
   * unspecified. Returns monostate on success with structured diagnostics;
   * config.transform_diagnostics is not applied.
   * @pre into must be non-null. Keep the tree and character storage alive
   * while using borrowed values or diagnostics. */
  OMNI_CONSTRAINED_TEMPLATE((typename T),
    (std::is_object<T>::value && !std::is_const<T>::value))
  auto operator()(T *into, const ::ryml::Tree &tree) const
    -> with_diagnostics_t<compat::monostate> {
    return (*this)(into, tree.crootref());
  }

  // Deleted: mapped values and diagnostics may borrow the tree's storage.
  OMNI_CONSTRAINED_TEMPLATE((typename T),
    (std::is_object<T>::value && !std::is_const<T>::value))
  void operator()(T *, const ::ryml::Tree &&) const = delete;

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
  // Ad hoc: shared default identity before C++17 inline variables.
  static const map_by_schema_t _default;
#endif
};

/** Schema-driven node/tree mapping with default configuration.
 * Returns structured diagnostics borrowing the caller's tree. */
#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline const map_by_schema_t<> map_by_schema{};
#else
static constexpr const map_by_schema_t<> &map_by_schema =
  map_by_schema_t<>::_default;
#endif

namespace detail {

// Shared parsing and diagnostics transformation for both destination forms
// and all config value categories. The return type constrains renderer calls.
template <typename T, typename Config, typename Destination>
auto deserialize(Config &&options, Destination destination, std::string source)
  -> with_diagnostics_t<T, diagnostics_result_t<Config>>;

} // namespace detail

/** JSON/YAML deserialization into C++ values with configurable diagnostics.
 * config.transform_diagnostics receives the live tree and collected issues;
 * its result accompanies the value. The default transformation renders text.
 * The parsed tree is released before return; mapped values and transformed
 * diagnostics must not retain views into it.
 * tdm_config returns a new callable with transformed configuration. */
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

  /** Deserialize source text into Record, returning transformed diagnostics.
   * Uses the mutable config, including lvalue-only diagnostic callbacks.
   * Value mappings construct Record directly; pointer-only mappings require
   * default construction.
   *
   * @warning Mapped values and transformed diagnostics must not retain views
   * into the parsed tree. Use core::parse with map_by_schema for borrowed
   * results. */
  OMNI_CONSTRAINED_TEMPLATE((typename Record),
    (std::is_object<Record>::value && !std::is_const<Record>::value))
  auto operator()(omni::type_t<Record> record_type, std::string source) &
    -> decltype(detail::deserialize<Record>(this->config, record_type,
      std::move(source))) {
    return detail::deserialize<Record>(this->config, record_type,
      std::move(source));
  }

  /** Deserialize source text into Record, returning transformed diagnostics.
   * Uses const config; the diagnostics callback must accept a const call.
   * Value mappings construct Record directly; pointer-only mappings require
   * default construction.
   *
   * @warning Mapped values and transformed diagnostics must not retain views
   * into the parsed tree. Use core::parse with map_by_schema for borrowed
   * results. */
  OMNI_CONSTRAINED_TEMPLATE((typename Record),
    (std::is_object<Record>::value && !std::is_const<Record>::value))
  auto operator()(omni::type_t<Record> record_type, std::string source) const &
    -> decltype(detail::deserialize<Record>(this->config, record_type,
      std::move(source))) {
    return detail::deserialize<Record>(this->config, record_type,
      std::move(source));
  }

  /** Deserialize source text into Record, returning transformed diagnostics.
   * Passes config as an rvalue, supporting consuming diagnostic callbacks.
   * Value mappings construct Record directly; pointer-only mappings require
   * default construction.
   *
   * @warning Mapped values and transformed diagnostics must not retain views
   * into the parsed tree. Use core::parse with map_by_schema for borrowed
   * results. */
  OMNI_CONSTRAINED_TEMPLATE((typename Record),
    (std::is_object<Record>::value && !std::is_const<Record>::value))
  auto operator()(omni::type_t<Record> record_type, std::string source) &&
    -> decltype(detail::deserialize<Record>(std::move(this->config),
      record_type, std::move(source))) {
    return detail::deserialize<Record>(std::move(this->config), record_type,
      std::move(source));
  }

  /** Deserialize source text into Record with const-rvalue configuration.
   * Returns transformed diagnostics, supporting const-rvalue-only callbacks.
   * Value mappings construct Record directly; pointer-only mappings require
   * default construction.
   *
   * @warning Mapped values and transformed diagnostics must not retain views
   * into the parsed tree. Use core::parse with map_by_schema for borrowed
   * results. */
  OMNI_CONSTRAINED_TEMPLATE((typename Record),
    (std::is_object<Record>::value && !std::is_const<Record>::value))
  auto operator()(omni::type_t<Record> record_type, std::string source) const &&
    -> decltype(detail::deserialize<Record>(std::move(this->config),
      record_type, std::move(source))) {
    return detail::deserialize<Record>(std::move(this->config), record_type,
      std::move(source));
  }

  /** Deserialize source text into *into with mutable config.
   * Returns monostate on success with transformed diagnostics. Ranges replace
   * existing contents. Syntax errors leave *into unchanged; mapping errors
   * leave its state unspecified and may produce partial updates.
   * @pre into must be non-null.
   *
   * @warning Mapped values and transformed diagnostics must not retain views
   * into the parsed tree. Use core::parse with map_by_schema for borrowed
   * results. */
  OMNI_CONSTRAINED_TEMPLATE((typename T),
    (std::is_object<T>::value && !std::is_const<T>::value))
  auto operator()(T *into, std::string source) &
    -> decltype(detail::deserialize<compat::monostate>(this->config, into,
      std::move(source))) {
    assert(into);
    return detail::deserialize<compat::monostate>(this->config, into,
      std::move(source));
  }

  /** Deserialize source text into *into with const config.
   * Returns monostate on success with transformed diagnostics. Ranges replace
   * existing contents. Syntax errors leave *into unchanged; mapping errors
   * leave its state unspecified and may produce partial updates.
   * @pre into must be non-null.
   *
   * @warning Mapped values and transformed diagnostics must not retain views
   * into the parsed tree. Use core::parse with map_by_schema for borrowed
   * results. */
  OMNI_CONSTRAINED_TEMPLATE((typename T),
    (std::is_object<T>::value && !std::is_const<T>::value))
  auto operator()(T *into, std::string source) const &
    -> decltype(detail::deserialize<compat::monostate>(this->config, into,
      std::move(source))) {
    assert(into);
    return detail::deserialize<compat::monostate>(this->config, into,
      std::move(source));
  }

  /** Deserialize source text into *into, passing config as an rvalue.
   * Returns monostate on success with transformed diagnostics. Ranges replace
   * existing contents. Syntax errors leave *into unchanged; mapping errors
   * leave its state unspecified and may produce partial updates.
   * @pre into must be non-null.
   *
   * @warning Mapped values and transformed diagnostics must not retain views
   * into the parsed tree. Use core::parse with map_by_schema for borrowed
   * results. */
  OMNI_CONSTRAINED_TEMPLATE((typename T),
    (std::is_object<T>::value && !std::is_const<T>::value))
  auto operator()(T *into, std::string source) &&
    -> decltype(detail::deserialize<compat::monostate>(std::move(this->config),
      into, std::move(source))) {
    assert(into);
    return detail::deserialize<compat::monostate>(std::move(this->config), into,
      std::move(source));
  }

  /** Deserialize source text into *into with const-rvalue configuration.
   * Returns monostate on success with transformed diagnostics. Ranges replace
   * existing contents. Syntax errors leave *into unchanged; mapping errors
   * leave its state unspecified and may produce partial updates.
   * @pre into must be non-null.
   *
   * @warning Mapped values and transformed diagnostics must not retain views
   * into the parsed tree. Use core::parse with map_by_schema for borrowed
   * results. */
  OMNI_CONSTRAINED_TEMPLATE((typename T),
    (std::is_object<T>::value && !std::is_const<T>::value))
  auto operator()(T *into, std::string source) const &&
    -> decltype(detail::deserialize<compat::monostate>(std::move(this->config),
      into, std::move(source))) {
    assert(into);
    return detail::deserialize<compat::monostate>(std::move(this->config), into,
      std::move(source));
  }

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
  // Ad hoc: shared default identity before C++17 inline variables.
  static const deserialize_t _default;
#endif
};

/** JSON/YAML deserialization with default config and text diagnostics. */
#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline const deserialize_t<> deserialize{};
#else
static constexpr const deserialize_t<> &deserialize =
  deserialize_t<>::_default;
#endif

// REFACTORME: [high] Expose type-switch routing for as_owning_tree_t<> and
// as_ref_tree_t<>, as map_by_schema_t does. A clean way to share the routing
// between these builders and mapping remains to be designed.
/** Owning RapidYAML tree serialization of supported C++ values.
 * Strings are copied into the tree's arena, so it can outlive the source. */
template <typename = void>
struct as_owning_tree_t {
  /** Serialize value into an owning tree. Empty optionals produce null;
   * empty optional sequence elements are omitted. The result can be modified
   * or passed to RapidYAML emitters. */
  template <typename T>
  ::ryml::Tree operator()(const T &value) const;

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
  // Ad hoc: pre-C++17 shared identity; constexpr definition needs a
  // complete self-type and therefore belongs in the implementation header.
  static const as_owning_tree_t _default;
#endif
};

/** Owning tree serialization; string storage is copied into the tree. */
#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr as_owning_tree_t<> as_owning_tree{};
#else
static constexpr const as_owning_tree_t<> &as_owning_tree =
  as_owning_tree_t<>::_default;
#endif

/** RapidYAML tree serialization borrowing the source's string storage.
 * The tree owns nodes and generated text. Source storage must remain alive
 * and unchanged while the tree is used. */
template <typename = void>
struct as_ref_tree_t {
  /** Serialize an lvalue into a tree borrowing its existing string storage.
   * Accepts const and mutable sources. Keep source storage alive and unchanged
   * while the tree is used. Empty optionals produce null; empty optional
   * sequence elements are omitted. */
  template <typename T>
  ::ryml::Tree operator()(T &value) const;

  // Deleted: a reference tree must not borrow from a temporary source.
  template <typename T>
  ::ryml::Tree operator()(const T &&) const = delete;

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
  // Ad hoc: pre-C++17 shared identity; constexpr definition needs a
  // complete self-type and therefore belongs in the implementation header.
  static const as_ref_tree_t _default;
#endif
};

/** Borrowing tree serialization; source strings must outlive the tree. */
#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr as_ref_tree_t<> as_ref_tree{};
#else
static constexpr const as_ref_tree_t<> &as_ref_tree =
  as_ref_tree_t<>::_default;
#endif

/** JSON/YAML text serialization with the selected .format. */
template <typename = void>
struct serialize_t {
  // Output format; use as_yaml or as_json for the predefined configurations.
  ::ryml::EmitType_e format;

  /**
   * Serialize a reflected record, supported scalar, or vector to YAML or JSON.
   * Vectors may contain nested vectors or reflected records, including at the
   * root. String sources include char-based basic_string and c4 views, plus
   * basic_string_view when the standard library provides it.
   * Empty optionals emit null at the root or as record fields. Empty optional
   * sequence elements are omitted; remaining elements keep their order.
   * Populated optionals serialize their contained value.
   * Strings are quoted and borrowed from the input until emission completes.
   * Keep their storage alive and unchanged until the call returns, including
   * mutable data members and storage exposed by custom iterators.
   * Reflected enums serialize as their declared names.
   */
  template <typename Value>
  std::string operator()(const Value &value) const;

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
  // Ad hoc: defer constexpr self-type storage until this class is complete.
  struct _default_storage {
    // Ad hoc: shared YAML default before C++17 inline variables.
    static constexpr serialize_t yaml{::ryml::EMIT_YAML};
    // Ad hoc: shared JSON default before C++17 inline variables.
    static constexpr serialize_t json{::ryml::EMIT_JSON};
  };
#endif
};

} // namespace ryml
} // namespace omni

// Complete nested storage definitions before binding the format defaults.
#include <omnirefl/serialization/ryml/impl.hpp>

namespace omni {
namespace ryml {

/**
 * Serialize reflected records, supported scalars, or vectors to YAML text.
 * Reads the value and its referenced storage through const references. Keep
 * them alive and unchanged until the call returns. Concurrent reads are safe;
 * synchronize any concurrent mutation or destruction of the input storage.
 *
```cpp
const auto yaml = as_yaml(value);
``` */
#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr serialize_t<> as_yaml{
  /*format=*/::ryml::EMIT_YAML,
};
#else
static constexpr const serialize_t<> &as_yaml =
  serialize_t<>::_default_storage::yaml;
#endif

/**
 * Serialize reflected records, supported scalars, or vectors to JSON text.
 * Reads the value and its referenced storage through const references. Keep
 * them alive and unchanged until the call returns. Concurrent reads are safe;
 * synchronize any concurrent mutation or destruction of the input storage.
 *
```cpp
const auto json = as_json(value);
``` */
#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr serialize_t<> as_json{
  /*format=*/::ryml::EMIT_JSON,
};
#else
static constexpr const serialize_t<> &as_json =
  serialize_t<>::_default_storage::json;
#endif

} // namespace ryml
} // namespace omni

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
