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
 * transformed through tdm_config. map_node selects local mappings throughout
 * serialization and deserialization, including recursive schema traversal.
 *
 * Compatible with C++11 and later; examples use C++20 syntax for brevity.
 * Guide: extras/extensions/serialization/guide/main.cpp.
 * C++11 examples: extras/extensions/serialization/tests/serialization.cpp.
 */

// FIXME: [low] Freestanding: public diagnostics and text APIs require hosted
// owning strings and vectors; provide an alternative representation.

// FIXME: [high] Report write-depth overflow instead of silently stopping;
// the node-returning pipeline has no diagnostic result (see impl/depth.hpp).

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/traits.hpp>
#include <omnirefl/reflected_scope.hpp>
#include <omnirefl/serialization/ryml/compose.hpp>
#include <omnirefl/serialization/ryml/core.hpp>
#include <omnirefl/serialization/ryml/customization.hpp>
#include <omnirefl/serialization/ryml/impl/tdm_binding.hpp>
#include <omnirefl/serialization/ryml/map_node.hpp>
#include <omnirefl/serialization/ryml/map_type.hpp>
#include <omnirefl/serialization/ryml/state.hpp>
#include <omnirefl/serialization/ryml/with_diagnostics.hpp>
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

template <typename T, typename = void>
struct reflection_target;

struct map_selected;
struct map_in_reflected_scope;

// reflected_call accepts the discovered record/enum, not its containers.
template <typename T>
using map_by_schema_dispatch_t = compat::conditional_t<
  cp::reflect_as_root<typename reflection_target<T>::type>::value,
  map_in_reflected_scope, map_selected>;

} // namespace detail

/** Schema-driven mapping of a caller-owned tree or subtree with config. */
template <typename Config = omni::ryml::config_t<>,
  typename MapNode = reflected_scope::mapping_to_type_t>
struct map_by_schema_t:
    detail::tdm_for_map_by_schema<Config, MapNode> {
  using tdm_provider = detail::tdm_for_map_by_schema<Config, MapNode>;

  Config config OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_config;

  /** Local mapping selection, propagated through recursive reads.
   * `map_node :: (state, type<T>) -> Mapping`
   * Mapping follows the cp::to_type callable contract. */
  MapNode map_node OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_map_node;

  /** Map a node into T by traversing its C++ schema over the node.
   *
```
map_by_schema :: (type<T>, node) -> with_diagnostics<T>
```
   *
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
      read_state_t<detail::read_state_config<compat::remove_cvref_t<Config>,
        compat::remove_cvref_t<MapNode>, T>>{
        /*config=*/this->config,
        /*error_count=*/0,
        /*warning_count=*/0,
        /*map_node=*/this->map_node,
      },

      node,
      type);
  }

  /** (QoL) Map a whole tree into T through its root node.
   *
```
map_by_schema :: (type<T>, tree) -> with_diagnostics<T>
```
   *
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
   *
```
map_by_schema :: (T*, node) -> with_diagnostics<monostate>
```
   *
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
      read_state_t<detail::read_state_config<compat::remove_cvref_t<Config>,
        compat::remove_cvref_t<MapNode>, T>>{
        /*config=*/this->config,
        /*error_count=*/0,
        /*warning_count=*/0,
        /*map_node=*/this->map_node,
      },

      node,
      into);
  }

  /** (QoL) Map a whole tree into *into through its root node.
   *
```
map_by_schema :: (T*, tree) -> with_diagnostics<monostate>
```
   *
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

  static_assert(is_config<Config>::value,
    "map_by_schema_t requires a specialization of omni::ryml::config_t");

// Ad hoc: classes with bases are not aggregates before C++17. These
// constructors preserve field initialization in the older standards.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr map_by_schema_t() = default;

  OMNI_CONSTRAINED_TEMPLATE((typename First = Config,
    typename Second = MapNode),
    (std::is_constructible<Config, First &&>::value
      && std::is_constructible<MapNode, Second &&>::value))
  constexpr map_by_schema_t(First &&first, Second &&second = Second{})
    : config(std::forward<First>(first)),
      map_node(std::forward<Second>(second)) {}
#endif
};

/** Schema-driven node/tree mapping with default configuration.
 * Returns structured diagnostics borrowing the caller's tree. */
#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline const map_by_schema_t<> map_by_schema{};
#else
static constexpr const map_by_schema_t<> &map_by_schema =
  compat::detail::shared_state<map_by_schema_t<>>::value;
#endif

namespace detail {

// Shared parsing and diagnostics transformation for both destination forms
// and all config value categories. The return type constrains renderer calls.
template <typename T, typename Config, typename MapNode, typename Destination>
auto deserialize(Config &&options, const MapNode &map_node,
  Destination destination, std::string source)
  -> with_diagnostics_t<T, diagnostics_result_t<Config>>;

} // namespace detail

/** JSON/YAML deserialization into C++ values with configurable diagnostics.
 * config.transform_diagnostics receives the live tree and collected issues;
 * its result accompanies the value. The default transformation renders text.
 * The parsed tree is released before return; mapped values and transformed
 * diagnostics must not retain views into it.
 * tdm_config returns a new callable with transformed configuration. */
template <typename Config = omni::ryml::config_t<>,
  typename MapNode = reflected_scope::mapping_to_type_t>
struct deserialize_t:
    detail::tdm_for_deserialize<Config, MapNode> {
  using tdm_provider = detail::tdm_for_deserialize<Config, MapNode>;

  Config config OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_config;

  /** Local mapping selection, propagated through recursive reads.
   * `map_node :: (state, type<T>) -> Mapping`
   * Mapping follows the cp::to_type callable contract. */
  MapNode map_node OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_map_node;

  /** Deserialize source text into Record, returning transformed diagnostics.
   *
```
deserialize :: (type<Record>, string) ->
  with_diagnostics<Record, diagnostics_result_t<Config>>
```
   *
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
    -> decltype(detail::deserialize<Record>(this->config,
      this->map_node, record_type, std::move(source))) {
    return detail::deserialize<Record>(this->config,
      this->map_node, record_type, std::move(source));
  }

  /** Deserialize source text into Record, returning transformed diagnostics.
   *
```
deserialize :: (type<Record>, string) ->
  with_diagnostics<Record, diagnostics_result_t<Config>>
```
   *
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
    -> decltype(detail::deserialize<Record>(this->config,
      this->map_node, record_type, std::move(source))) {
    return detail::deserialize<Record>(this->config,
      this->map_node, record_type, std::move(source));
  }

  /** Deserialize source text into Record, returning transformed diagnostics.
   *
```
deserialize :: (type<Record>, string) ->
  with_diagnostics<Record, diagnostics_result_t<Config>>
```
   *
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
      this->map_node, record_type, std::move(source))) {
    return detail::deserialize<Record>(std::move(this->config),
      this->map_node, record_type, std::move(source));
  }

  /** Deserialize source text into Record with const-rvalue configuration.
   *
```
deserialize :: (type<Record>, string) ->
  with_diagnostics<Record, diagnostics_result_t<Config>>
```
   *
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
      this->map_node, record_type, std::move(source))) {
    return detail::deserialize<Record>(std::move(this->config),
      this->map_node, record_type, std::move(source));
  }

  /** Deserialize source text into *into with mutable config.
   *
```
deserialize :: (T*, string) ->
  with_diagnostics<monostate, diagnostics_result_t<Config>>
```
   *
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
    -> decltype(detail::deserialize<compat::monostate>(this->config,
      this->map_node, into, std::move(source))) {
    assert(into);
    return detail::deserialize<compat::monostate>(this->config,
      this->map_node, into, std::move(source));
  }

  /** Deserialize source text into *into with const config.
   *
```
deserialize :: (T*, string) ->
  with_diagnostics<monostate, diagnostics_result_t<Config>>
```
   *
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
    -> decltype(detail::deserialize<compat::monostate>(this->config,
      this->map_node, into, std::move(source))) {
    assert(into);
    return detail::deserialize<compat::monostate>(this->config,
      this->map_node, into, std::move(source));
  }

  /** Deserialize source text into *into, passing config as an rvalue.
   *
```
deserialize :: (T*, string) ->
  with_diagnostics<monostate, diagnostics_result_t<Config>>
```
   *
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
      this->map_node, into, std::move(source))) {
    assert(into);
    return detail::deserialize<compat::monostate>(std::move(this->config),
      this->map_node, into, std::move(source));
  }

  /** Deserialize source text into *into with const-rvalue configuration.
   *
```
deserialize :: (T*, string) ->
  with_diagnostics<monostate, diagnostics_result_t<Config>>
```
   *
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
      this->map_node, into, std::move(source))) {
    assert(into);
    return detail::deserialize<compat::monostate>(std::move(this->config),
      this->map_node, into, std::move(source));
  }

  static_assert(is_config<Config>::value,
    "deserialize_t requires a specialization of omni::ryml::config_t");

// Ad hoc: classes with bases are not aggregates before C++17. These
// constructors preserve field initialization in the older standards.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr deserialize_t() = default;

  OMNI_CONSTRAINED_TEMPLATE((typename First = Config,
    typename Second = MapNode),
    (std::is_constructible<Config, First &&>::value
      && std::is_constructible<MapNode, Second &&>::value))
  constexpr deserialize_t(First &&first, Second &&second = Second{})
    : config(std::forward<First>(first)),
      map_node(std::forward<Second>(second)) {}
#endif
};

/** JSON/YAML deserialization with default config and text diagnostics. */
#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline const deserialize_t<> deserialize{};
#else
static constexpr const deserialize_t<> &deserialize =
  compat::detail::shared_state<deserialize_t<>>::value;
#endif

// REFACTORME: [high] Expose type-switch routing for as_owning_tree_t and
// as_ref_tree_t, as map_by_schema_t does. A clean way to share the routing
// between these builders and mapping remains to be designed.
/** Owning RapidYAML tree serialization of supported C++ values.
 * Strings are copied into the tree's arena, so it can outlive the source. */
template <typename MapNode = reflected_scope::mapping_from_type_t>
struct as_owning_tree_t:
    detail::tdm_for_as_owning_tree<MapNode> {
  using tdm_provider = detail::tdm_for_as_owning_tree<MapNode>;

  // (state, type<T>) -> mapping callable, selected at every schema position.
  MapNode map_node OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_map_node;

  /** Shared empty-optional policy; see \ref write_config. */
  write_config config{};
  using tdm_provider::tdm_config;

  /** Serialize value into an owning tree.
   *
```
as_owning_tree :: const T& -> tree
```
   *
   * By default, empty optionals produce null;
   * empty optional sequence elements are omitted. The result can be modified
   * or passed to RapidYAML emitters. */
  template <typename T>
  ::ryml::Tree operator()(const T &value) const;

// Ad hoc: classes with bases are not aggregates before C++17. These
// constructors preserve field initialization in the older standards.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr as_owning_tree_t() = default;

  OMNI_CONSTRAINED_TEMPLATE((typename Value = MapNode,
    typename Options = write_config),
    (std::is_constructible<MapNode, Value &&>::value
      && std::is_constructible<write_config, Options &&>::value))
  constexpr as_owning_tree_t(Value &&value, Options &&options = Options{})
    : map_node(std::forward<Value>(value)),
      config(std::forward<Options>(options)) {}
#endif
};

/** Owning tree serialization; string storage is copied into the tree. */
#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr as_owning_tree_t<> as_owning_tree{};
#else
static constexpr const as_owning_tree_t<> &as_owning_tree =
  compat::detail::shared_state<as_owning_tree_t<>>::value;
#endif

/** RapidYAML tree serialization borrowing the source's string storage.
 * The tree owns nodes and generated text. Source storage must remain alive
 * and unchanged while the tree is used. */
template <typename MapNode = reflected_scope::mapping_from_type_t>
struct as_ref_tree_t:
    detail::tdm_for_as_ref_tree<MapNode> {
  using tdm_provider = detail::tdm_for_as_ref_tree<MapNode>;

  // (state, type<T>) -> mapping callable, selected at every schema position.
  MapNode map_node OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_map_node;

  /** Shared empty-optional policy; see \ref write_config. */
  write_config config{};
  using tdm_provider::tdm_config;

  /** Serialize an lvalue into a tree borrowing its existing string storage.
   *
```
as_ref_tree :: T& -> tree
```
   *
   * Accepts const and mutable sources. Keep source storage alive and unchanged
   * while the tree is used. By default, empty optionals produce null;
   * empty optional sequence elements are omitted. */
  template <typename T>
  ::ryml::Tree operator()(T &value) const;

  // Deleted: a reference tree must not borrow from a temporary source.
  template <typename T>
  ::ryml::Tree operator()(const T &&) const = delete;

// Ad hoc: classes with bases are not aggregates before C++17. These
// constructors preserve field initialization in the older standards.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr as_ref_tree_t() = default;

  OMNI_CONSTRAINED_TEMPLATE((typename Value = MapNode,
    typename Options = write_config),
    (std::is_constructible<MapNode, Value &&>::value
      && std::is_constructible<write_config, Options &&>::value))
  constexpr as_ref_tree_t(Value &&value, Options &&options = Options{})
    : map_node(std::forward<Value>(value)),
      config(std::forward<Options>(options)) {}
#endif
};

/** Borrowing tree serialization; source strings must outlive the tree. */
#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr as_ref_tree_t<> as_ref_tree{};
#else
static constexpr const as_ref_tree_t<> &as_ref_tree =
  compat::detail::shared_state<as_ref_tree_t<>>::value;
#endif

/** JSON/YAML text serialization with the selected .format. */
template <typename MapNode = reflected_scope::mapping_from_type_t>
struct serialize_t:
    detail::tdm_for_serialize<MapNode> {
  using tdm_provider = detail::tdm_for_serialize<MapNode>;

  // Output format; use as_yaml or as_json for the predefined configurations.
  ::ryml::EmitType_e format{};
  using tdm_provider::tdm_format;

  // (state, type<T>) -> mapping callable, selected at every schema position.
  MapNode map_node OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_map_node;

  /** Shared empty-optional policy; see \ref write_config. */
  write_config config{};
  using tdm_provider::tdm_config;

  /** Serialize a reflected record, supported scalar, or vector to YAML or JSON.
   *
```
serialize :: const T& -> string
```
   *
   * Vectors may contain nested vectors or reflected records, including at the
   * root. String sources include char-based basic_string and c4 views, plus
   * basic_string_view when the standard library provides it.
   * By default, empty optionals emit null at the root or as record fields;
   * empty optional sequence elements are omitted. Remaining elements keep
   * their order.
   * Populated optionals serialize their contained value.
   * Strings are quoted and borrowed from the input until emission completes.
   * Keep their storage alive and unchanged until the call returns, including
   * mutable data members and storage exposed by custom iterators.
   * Reflected enums serialize as their declared names.
   */
  template <typename Value>
  std::string operator()(const Value &value) const;

// Ad hoc: classes with bases are not aggregates before C++17. These
// constructors preserve field initialization in the older standards.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr serialize_t() = default;

  OMNI_CONSTRAINED_TEMPLATE((typename First = ::ryml::EmitType_e,
    typename Second = MapNode, typename Options = write_config),
    (std::is_constructible<::ryml::EmitType_e, First &&>::value
      && std::is_constructible<MapNode, Second &&>::value
      && std::is_constructible<write_config, Options &&>::value))
  constexpr serialize_t(First &&first, Second &&second = Second{},
    Options &&options = Options{})
    : format(std::forward<First>(first)),
      map_node(std::forward<Second>(second)),
      config(std::forward<Options>(options)) {}
#endif
};

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
namespace detail {

// Ad hoc: distinct configured defaults of one type before inline variables.
struct serialization_defaults {
  serialize_t<> yaml{::ryml::EMIT_YAML};
  serialize_t<> json{::ryml::EMIT_JSON};
};

} // namespace detail
#endif

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
  /*base=*/{},
  /*format=*/::ryml::EMIT_YAML,
};
#else
static constexpr const serialize_t<> &as_yaml =
  compat::detail::shared_state<detail::serialization_defaults>::value.yaml;
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
  /*base=*/{},
  /*format=*/::ryml::EMIT_JSON,
};
#else
static constexpr const serialize_t<> &as_json =
  compat::detail::shared_state<detail::serialization_defaults>::value.json;
#endif

/** JSON serialization with configurable format and local mapping selection.
 * tdm_format returns a binding with a transformed format; as_json and as_yaml
 * provide the predefined formats. Input storage stays alive through the call.
 *
```cpp
const auto json = serialize(value);
``` */
#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr serialize_t<> serialize{
  /*base=*/{},
  /*format=*/::ryml::EMIT_JSON,
};
#else
static constexpr const serialize_t<> &serialize =
  compat::detail::shared_state<detail::serialization_defaults>::value.json;
#endif

} // namespace ryml
} // namespace omni

// Definitions follow the complete public interface.
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
