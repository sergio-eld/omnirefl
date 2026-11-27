#pragma once

/** Internal schema paths, state configuration, and terminal-child selection. */

#include <omnirefl/extensions/static_string.hpp>
#include <omnirefl/extensions/traits.hpp>

#include <cstddef>

namespace omni {
namespace ryml {

struct depth_exceeded;

template <typename StateConfig>
struct read_state_t;

template <typename StateConfig>
struct write_state_t;

namespace reflected_scope {
struct mapping_to_type_t;
struct mapping_from_type_t;
} // namespace reflected_scope

namespace detail {

// C++11 lookup results are defined in customization.hpp after mapper types.
template <typename T>
struct to_type_cpp11_trailing_return;

template <typename T>
struct from_type_cpp11_trailing_return;

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

// FIXME: [high] Reduce recursive path/depth specializations. For example:
//   struct node {
//     std::vector<node> children, alternatives, fallbacks;
//   };
// Different field orders form distinct paths; each depth adds another state
// specialization. The terminal sentinel bounds this expansion but does not
// prevent excessive compiler memory use before the limit, on reads or writes.
// Re-entering the same reflected field revisits a recursive schema edge.
// Reuse its first schema path rather than instantiate unbounded state types.
template <typename Prefix, typename Segment, typename... Remaining>
struct schema_field_path;

template <typename... Prefix, typename Segment>
struct schema_field_path<schema_path<Prefix...>, Segment> {
  using type = schema_path<Prefix..., Segment>;
};

template <typename... Prefix, typename Segment, typename... Tail>
struct schema_field_path<schema_path<Prefix...>, Segment, Segment, Tail...> {
  using type = schema_path<Prefix..., Segment>;
};

template <typename... Prefix, typename Segment, typename Head, typename... Tail>
struct schema_field_path<schema_path<Prefix...>, Segment, Head, Tail...>:
    schema_field_path<schema_path<Prefix..., Head>, Segment, Tail...> {};

template <typename... Segments>
struct schema_path {
  template <typename Name>
  using child = schema_path<Segments..., field_path_name<Name>>;

  template <typename Field>
  using field = typename schema_field_path<schema_path<>,
    field_path_name<name_as_string<Field>>, Segments...>::type;

  using element = schema_path<Segments..., path_element>;
  using text = cpp11_static_string<Segments...>;
};

template <typename T,
  bool Optional = traits::is_optional<T>::value,
  bool KeyValue = traits::is_key_value<T>::value>
struct discovered_range_type {
  using type = compat::remove_cvref_t<compat::range_value_t<T>>;
};

template <typename T, bool KeyValue>
struct discovered_range_type<T, true, KeyValue> {
  using type = compat::remove_cvref_t<typename T::value_type>;
};

template <typename T>
struct discovered_range_type<T, false, true> {
  using type = compat::remove_cvref_t<traits::mapped_type_t<T>>;
};

// Named custom children may have no declared schema type. Their selected
// default range mapper supplies it before descending; reflected fields and
// ordinary range elements retain their declared type directly.
template <typename T, typename = void>
struct schema_element_type { using type = void; };

template <typename T>
struct schema_element_type<T, compat::void_t<compat::range_value_t<T>>>:
    discovered_range_type<T> {};

template <typename T>
struct state_schema;

// Omission infers the path parent but retains the legacy enclosing_record.
// An explicit parent overrides both, even when it equals enclosing_record.
struct inferred_schema_parent {};

// Type-only bundles keep state friendship independent of parameter-list arity.
template <typename Config,
  typename SelectMapping = reflected_scope::mapping_to_type_t,
  typename Root = void, typename EnclosingRecord = void,
  typename Path = schema_path<root_path_name>, std::size_t Depth = 0,
  std::size_t DepthLimit = 10, typename SchemaParent = EnclosingRecord,
  typename Schema = Root>
struct read_state_config {
  using config_type = Config;
  using select_mapping_type = SelectMapping;
  using enclosing_root = Root;
  using enclosing_record = EnclosingRecord;
  using schema_parent = SchemaParent;
  using path_type = Path;
  static constexpr std::size_t depth = Depth;
  static constexpr std::size_t depth_limit = DepthLimit;

  template <typename Record>
  using record_parent = compat::conditional_t<
    std::is_same<Record, inferred_schema_parent>::value,
    EnclosingRecord, Record>;

  template <typename Record>
  using path_parent = compat::conditional_t<
    std::is_same<Record, inferred_schema_parent>::value,
    compat::conditional_t<std::is_void<Schema>::value, EnclosingRecord, Schema>,
    Record>;

  template <typename Record, typename Name>
  using child = read_state_config<Config, SelectMapping, Root,
    record_parent<Record>,
    typename Path::template child<Name>, Depth + 1, DepthLimit,
    path_parent<Record>, void>;

  template <typename Record, typename Field>
  using field = read_state_config<Config, SelectMapping, Root,
    record_parent<Record>,
    typename Path::template field<Field>, Depth + 1, DepthLimit,
    path_parent<Record>, typename Field::type>;

  template <typename T>
  using at_schema = read_state_config<Config, SelectMapping, Root,
    EnclosingRecord, Path, Depth, DepthLimit, SchemaParent, T>;

  using element = read_state_config<Config, SelectMapping, Root,
    EnclosingRecord, typename Path::element, Depth + 1, DepthLimit, Schema,
    typename schema_element_type<Schema>::type>;
};

template <bool BorrowStrings = false,
  typename SelectMapping = reflected_scope::mapping_from_type_t,
  typename Root = void, typename EnclosingRecord = void,
  typename Path = schema_path<root_path_name>, std::size_t Depth = 0,
  std::size_t DepthLimit = 10, typename SchemaParent = EnclosingRecord,
  typename Schema = Root>
struct write_state_config {
  static constexpr bool borrow_strings = BorrowStrings;
  using select_mapping_type = SelectMapping;
  using enclosing_root = Root;
  using enclosing_record = EnclosingRecord;
  using schema_parent = SchemaParent;
  using path_type = Path;
  static constexpr std::size_t depth = Depth;
  static constexpr std::size_t depth_limit = DepthLimit;

  template <typename Record>
  using record_parent = compat::conditional_t<
    std::is_same<Record, inferred_schema_parent>::value,
    EnclosingRecord, Record>;

  template <typename Record>
  using path_parent = compat::conditional_t<
    std::is_same<Record, inferred_schema_parent>::value,
    compat::conditional_t<std::is_void<Schema>::value, EnclosingRecord, Schema>,
    Record>;

  template <typename Record, typename Name>
  using child = write_state_config<BorrowStrings, SelectMapping, Root,
    record_parent<Record>,
    typename Path::template child<Name>, Depth + 1, DepthLimit,
    path_parent<Record>, void>;

  template <typename Record, typename Field>
  using field = write_state_config<BorrowStrings, SelectMapping, Root,
    record_parent<Record>,
    typename Path::template field<Field>, Depth + 1, DepthLimit,
    path_parent<Record>, typename Field::type>;

  template <typename T>
  using at_schema = write_state_config<BorrowStrings, SelectMapping, Root,
    EnclosingRecord, Path, Depth, DepthLimit, SchemaParent, T>;

  using element = write_state_config<BorrowStrings, SelectMapping, Root,
    EnclosingRecord, typename Path::element, Depth + 1, DepthLimit, Schema,
    typename schema_element_type<Schema>::type>;
};

// Select the terminal token before forming any child configuration or path.
template <template <typename> class State, typename StateConfig,
  bool CanDescend = (StateConfig::depth < StateConfig::depth_limit)>
struct state_children {
  template <typename Record, typename Name>
  using child = State<typename StateConfig::template child<Record, Name>>;

  template <typename Record, typename Field>
  using field = State<typename StateConfig::template field<Record, Field>>;

  using element = State<typename StateConfig::element>;
};

template <template <typename> class State, typename StateConfig>
struct state_children<State, StateConfig, false> {
  template <typename Record, typename Name>
  using child = depth_exceeded;

  template <typename Record, typename Field>
  using field = depth_exceeded;

  using element = depth_exceeded;
};

// Selection knows the current C++ type. Retain it for parameterless element()
// and for default range mapping delegated to by a custom mapper.
template <typename T>
struct state_schema {
  template <typename Config>
  static constexpr auto bind(const read_state_t<Config> &state)
    -> read_state_t<typename Config::template at_schema<T>> {
    return {state};
  }

  template <typename Config>
  static constexpr auto bind(const write_state_t<Config> &state)
    -> write_state_t<typename Config::template at_schema<T>> {
    return {state};
  }
};

} // namespace detail
} // namespace ryml
} // namespace omni
