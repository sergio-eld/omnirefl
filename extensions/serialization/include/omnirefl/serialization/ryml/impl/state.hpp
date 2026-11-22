#pragma once

/** Internal schema paths, state configuration, and terminal-child selection. */

#include <omnirefl/extensions/static_string.hpp>

#include <cstddef>

namespace omni {
namespace ryml {

struct depth_exceeded;

namespace reflected_scope {
struct mapping_to_type_t;
struct mapping_from_type_t;
} // namespace reflected_scope

namespace detail {

// The C++11 result is defined after the built-in mapping types are complete.
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

  // Default traversal needs segment identity only. Render characters when a
  // callback queries path(), rather than while forming every child state.
  static constexpr auto string() OMNI_CPP11_TRAILING_RETURN(text) {
#if OMNI_HAS_NTTP_STRING
    return nttp_string<omni::detail::static_string_literal<
      text::size() + 1>{text::view()}>{};
#else
    return text{};
#endif
  }
};

// Type-only bundles keep state friendship independent of parameter-list arity.
template <typename Config,
  typename MapNode = reflected_scope::mapping_to_type_t,
  typename Root = void, typename EnclosingRecord = void,
  typename Path = schema_path<root_path_name>, std::size_t Depth = 0,
  std::size_t DepthLimit = 10>
struct read_state_config {
  using config_type = Config;
  using map_node_type = MapNode;
  using enclosing_root = Root;
  using enclosing_record = EnclosingRecord;
  using path_type = Path;
  static constexpr std::size_t depth = Depth;
  static constexpr std::size_t depth_limit = DepthLimit;

  template <typename Record, typename Name>
  using child = read_state_config<Config, MapNode, Root, Record,
    typename Path::template child<Name>, Depth + 1, DepthLimit>;

  template <typename Record, typename Field>
  using field = read_state_config<Config, MapNode, Root, Record,
    typename Path::template field<Field>, Depth + 1, DepthLimit>;

  using element = read_state_config<Config, MapNode, Root, EnclosingRecord,
    typename Path::element, Depth + 1, DepthLimit>;
};

template <bool BorrowStrings = false,
  typename MapNode = reflected_scope::mapping_from_type_t,
  typename Root = void, typename EnclosingRecord = void,
  typename Path = schema_path<root_path_name>, std::size_t Depth = 0,
  std::size_t DepthLimit = 10>
struct write_state_config {
  static constexpr bool borrow_strings = BorrowStrings;
  using map_node_type = MapNode;
  using enclosing_root = Root;
  using enclosing_record = EnclosingRecord;
  using path_type = Path;
  static constexpr std::size_t depth = Depth;
  static constexpr std::size_t depth_limit = DepthLimit;

  template <typename Record, typename Name>
  using child = write_state_config<BorrowStrings, MapNode, Root, Record,
    typename Path::template child<Name>, Depth + 1, DepthLimit>;

  template <typename Record, typename Field>
  using field = write_state_config<BorrowStrings, MapNode, Root, Record,
    typename Path::template field<Field>, Depth + 1, DepthLimit>;

  using element = write_state_config<BorrowStrings, MapNode, Root,
    EnclosingRecord, typename Path::element, Depth + 1, DepthLimit>;
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

} // namespace detail

} // namespace ryml
} // namespace omni
