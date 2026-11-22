#pragma once

/** Shared mapping configuration, diagnostics, and read/write states. */

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/serialization/ryml/impl/state.hpp>
#include <omnirefl/serialization/ryml/impl/tdm_config.hpp>
#include <omnirefl/traits.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <string>
#include <utility>
#include <vector>

namespace omni {

template <typename T>
struct type_t;

// Only backend-independent JSON/YAML mapping contracts belong here.
namespace json {

/** Member-name mapping contract.
 * Each mapped member has a canonical name and may accept aliases. The sets
 * consisting of each member's canonical name and aliases must be disjoint
 * across members. Shared strategy dictionaries provide valid_aliases checks;
 * opaque callable resolvers retain responsibility for this precondition.
 *
 * If any canonical occurrence exists, its aliases are ignored regardless of
 * value validity. Otherwise aliases identify the member. Duplicate policies
 * select among canonical occurrences or, only when absent, among aliases.
 * Only the selected occurrence is decoded; invalid canonical input never
 * falls back to an alias. Present but invalid input is not a missing member.
 */

/** JSON/YAML parsing and mapping failures, independent of the parser backend.
 * Field presence and diagnostic limits follow the requested mapping contract.
 */
enum class issue_code : std::uint8_t {
  unexpected_kind,
  invalid_scalar,
  out_of_range, //< Value lies outside the range accepted by the mapping.
  missing_field,
  unknown_field,
  duplicate_field,
  parse_error,
  warning_limit_exceeded,
  duplicate_element, //< Repeated value in a unique-element destination.
  depth_limit_exceeded //< Traversal would exceed the configured tree depth.
};

/** Value categories used in mapping expectations and observed input.
 * These describe values, not destination-language storage types. */
enum class value_kind : std::uint8_t {
  unknown, //< No category is known or applicable.
  scalar, //< A non-collection value whose category has not been resolved.
  mapping,
  sequence,
  string,
  boolean,
  integer, //< An integral number.
  unsigned_integer, //< A non-negative integral number.
  number,
  null
};

/** Controls repeated object names and elements of unique destinations.
 * Last implies allow; warn is ignored when duplicates are rejected. */
enum duplicate_policy : std::uint8_t {
  reject = 0,
  allow = 1 << 0, //< Keep the first occurrence, without a warning.
  last = 1 << 1, //< Keep the last occurrence.
  warn = 1 << 2 //< Warn about repeated field names when duplicates are allowed.
};

OMNI_ENABLE_BIT_FLAGS(duplicate_policy)

/** Input fields absent from the struct; warn is ignored without allow. */
enum class unknown_policy : std::uint8_t {
  reject = 0,
  allow = 1 << 0, //< Ignore unknown fields without a warning.
  warn = 1 << 1 //< Warn about unknown fields only when allow is set.
};

OMNI_ENABLE_BIT_FLAGS(unknown_policy)

/**
 * Controls mapping errors independently of the JSON or YAML parsing algorithm.
 *
 * @details
 * Mapping returns with_diagnostics_t: a valid
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
  compat::optional<std::uint16_t> warning_limit{};

  // Reject duplicates unless allow or last is set; warn alone does not allow.
  duplicate_policy duplicates = duplicate_policy::reject;

  // Reject input fields absent from the struct by default.
  unknown_policy unknown_fields = unknown_policy::reject;
};

} // namespace json

namespace ryml {

struct diagnostics;

template <typename Config, typename MapNode>
struct map_by_schema_t;

template <typename MapNode>
struct as_owning_tree_t;

template <typename MapNode>
struct as_ref_tree_t;

/** Render issue records using their source tree while it is alive. */
struct render_diagnostics_t {
  std::string operator()(const ::ryml::Tree &tree, const diagnostics &d) const;
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr render_diagnostics_t render_diagnostics{};
#else
static constexpr const render_diagnostics_t &render_diagnostics =
  compat::detail::shared_state<render_diagnostics_t>::value;
#endif

/** Deserialization configuration.
 * In C++20, aggregate deduction supports config_t{.transform_diagnostics = f}.
 * Earlier standards can use tdm_transform_diagnostics to change the callable
 * type without spelling the resulting config specialization. */
template <typename TransformDiagnostics = render_diagnostics_t>
struct config_t:
    detail::tdm_for_config<TransformDiagnostics> {
  using tdm_provider = detail::tdm_for_config<TransformDiagnostics>;

  json::error_policy error_policy OMNI_CPP14_DEFAULT_INIT();

  // Called with the live source tree and collected diagnostics on every result.
  TransformDiagnostics transform_diagnostics OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_transform_diagnostics;

// Ad hoc: classes with bases are not aggregates before C++17. These
// constructors preserve field initialization in the older standards.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr config_t() = default;

  OMNI_CONSTRAINED_TEMPLATE((typename First = json::error_policy,
    typename Second = TransformDiagnostics),
    (std::is_constructible<json::error_policy, First &&>::value
      && std::is_constructible<TransformDiagnostics, Second &&>::value))
  constexpr config_t(First &&first, Second &&second = Second{})
    : error_policy(std::forward<First>(first)),
      transform_diagnostics(std::forward<Second>(second)) {}
#endif
};

/** Whether T is a config_t specialization, ignoring cv/ref qualifiers. */
template <typename T>
struct is_config:
    compat::bool_constant<traits::is_specialization_of<config_t, T>()> {};

// C++14 adds variable templates for trait values.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <typename T>
constexpr bool is_config_v = is_config<T>::value;
#endif

// C++20 callers can constrain configuration arguments directly.
#if defined(__cpp_concepts) && 201907L <= __cpp_concepts
template <typename T>
concept config_like = is_config<T>::value;
#endif

/** Describes an input issue without rendering a message or deciding failure. */
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
  std::vector<path_segment> path;

  // node_id refers to the caller-owned tree; field views may borrow its keys.
  // Missing fields may have no corresponding node.
  ::ryml::id_type node_id = ::ryml::NONE;

  // TODO: Revisit path storage if C++11 constant evaluation becomes necessary.
  // TODO: Accept retained parser source-location metadata when mapping;
  // standalone trees do not retain source positions.
  // TODO: Add constraint metadata when needed.
};

/** Collected mapping issues without ownership of their source tree.
 * Keep the tree and borrowed character storage alive and unchanged while
 * rendering diagnostics. Field paths may borrow metadata or source keys. */
struct diagnostics {
  // Mapping-policy snapshot; the deserialize transformation is not retained.
  omni::ryml::config_t<> config;

  // Validation and conversion issues in traversal order, including warnings.
  std::vector<issue> issues;

  // Owns RapidYAML syntax-error text returned by core::parse.
  // Empty when mapping an already parsed tree.
  std::string parse_error_message;
};

/** Diagnostics result produced by Config, preserving its cv/ref qualifiers. */
template <typename Config>
using diagnostics_result_t = compat::decay_t<compat::invoke_result_t<
  decltype((std::declval<Config>().transform_diagnostics)),
  const ::ryml::Tree &,
  diagnostics>>;

template <typename StateConfig>
struct read_state_t;

template <typename StateConfig>
struct write_state_t;

/** Terminal traversal token. Returned instead of a child state when
 * another edge would exceed the tree-depth limit. It carries no path and
 * cannot create further children. Reads report an error; writes currently
 * stop without reporting one (see the write overload's FIXME). */
struct depth_exceeded {
  // C++11 return-by-value requires an accessible copy constructor.
  constexpr depth_exceeded(const depth_exceeded &) = default;

private:
  template <typename>
  friend struct read_state_t;

  template <typename>
  friend struct write_state_t;

  // Ordinary states must not match terminal overloads through conversion.
  template <typename StateConfig>
  explicit constexpr depth_exceeded(const read_state_t<StateConfig> &) {}

  template <typename StateConfig>
  explicit constexpr depth_exceeded(const write_state_t<StateConfig> &) {}
};

namespace reflected_scope {

/** Callable form of the node-to-type customization lookup.
 *
```
mapping_to_type :: (state, type<T>) -> Mapping
```
 * State is accepted for compatibility with local mapping selectors.
 * The default selection depends only on T. */
struct mapping_to_type_t {
  template <typename State, typename T>
  OMNI_CPP14_CONSTEXPR auto operator()(const State &, omni::type_t<T>) const
    OMNI_CPP11_TRAILING_RETURN(
      typename omni::ryml::detail::to_type_cpp11_trailing_return<T>::type);
};

/** Callable form of the type-to-node customization lookup.
 *
```
mapping_from_type :: (state, type<T>) -> Mapping
```
 * The default selection depends only on T. */
struct mapping_from_type_t {
  template <typename State, typename T>
  OMNI_CPP14_CONSTEXPR auto operator()(const State &, omni::type_t<T>) const
    OMNI_CPP11_TRAILING_RETURN(
      typename omni::ryml::detail::from_type_cpp11_trailing_return<T>::type);
};

} // namespace reflected_scope

/** Deserialization configuration and traversal progress passed to readers.
 * Root states are created by map_by_schema; custom mappings copy the supplied
 * state or derive children. Config and the selector are borrowed, so copies
 * do not copy their contents or allocate storage.
 * Counters include issues reported by preceding operations in the enclosing
 * traversal. enclosing_root is the original root type; enclosing_record is
 * the immediate containing record, or void at the root. Containers and
 * optionals retain their containing record. The path describes the C++
 * schema, independently of input keys.
 * Mapping callbacks take a generic State, constrained by is_read_state,
 * because child paths and local selectors produce different state types.
 * Path-aware selectors take State by value so path() remains a constant
 * expression in C++17/20; config and the selector are still borrowed.
 * Same-node indirections forward the existing state. Custom traversal derives
 * a child state for each edge: child(OMNI_META_STRING("field")) for a named
 * field, field(meta) for a reflected field, or element() for container values.
 *
```cpp
// Inside a generic mapping selector, from C++17:
if constexpr (state.path().ends_with("root.work_address")) {
  // Select the work-address mapping; other address fields keep their defaults.
}
```
 */
template <typename StateConfig = detail::read_state_config<config_t<>>>
struct read_state_t {
  using enclosing_root = typename StateConfig::enclosing_root;
  using enclosing_record = typename StateConfig::enclosing_record;

  const typename StateConfig::config_type &config;
  std::size_t error_count;
  std::size_t warning_count;

  // (state, type<T>) -> mapping callable; borrowed throughout traversal.
  const typename StateConfig::map_node_type &map_node;

  /** Tree depth, starting at zero for the root.
   * child(), field(), and element() increment it; optional unwrapping does not.
   * At the limit, these operations return depth_exceeded instead of a state. */
  static constexpr std::size_t depth() { return StateConfig::depth; }

  /** Maximum tree depth, defaulting to 10. */
  static constexpr std::size_t depth_limit() {
    return StateConfig::depth_limit;
  }

  /** Canonical schema path: root.field, with [] for container values.
   * Independent of runtime keys, indices, and input aliases. Recursive schema
   * edges reuse their first path; this is not a runtime ancestry trace. */
  static constexpr auto path()
    OMNI_CPP11_TRAILING_RETURN(typename StateConfig::path_type::text) {
    return StateConfig::path_type::string();
  }

  /** Child state from a string token: child(OMNI_META_STRING("address"))
   * appends .address. Every call appends; reflected-field identity is unknown.
   * Record sets enclosing_record; omitted, the current record is retained.
   * At depth_limit(), returns depth_exceeded instead. */
  template <typename Record = enclosing_record, typename Name>
  constexpr auto child(Name) const
    -> typename detail::state_children<read_state_t, StateConfig>::template
      child<Record, Name> {
    return typename detail::state_children<read_state_t, StateConfig>::template
      child<Record, Name>{*this};
  }

  /** Child state from reflected metadata: appends `.` then field.name().
   * Repeating the same field metadata reuses its first path, removing the
   * intervening suffix to keep recursive schema paths finite.
   * Record sets enclosing_record; omitted, the current record is retained.
   * At depth_limit(), returns depth_exceeded instead. */
  template <typename Record = enclosing_record, typename Field>
  constexpr auto field(Field) const
    -> typename detail::state_children<read_state_t, StateConfig>::template
      field<Record, Field> {
    return typename detail::state_children<read_state_t, StateConfig>::template
      field<Record, Field>{*this};
  }

  /** Container-value state: root.items becomes root.items[].
   * Indices and mapping keys share []; enclosing_record is retained.
   * At depth_limit(), returns depth_exceeded instead. */
  constexpr auto element() const
    -> typename detail::state_children<read_state_t, StateConfig>::element {
    return typename detail::state_children<read_state_t, StateConfig>::element{
      *this};
  }

  /** Whether an error or warning budget has been exceeded. */
  constexpr bool limit_exceeded() const {
    return this->config.error_policy.error_limit < this->error_count
      || (this->config.error_policy.warning_limit
        && *this->config.error_policy.warning_limit < this->warning_count);
  }

  // Rvalue propagation copies this borrowed traversal token as well.
  constexpr read_state_t(const read_state_t &) = default;

private:
  // Child states use other configurations and need constructor access.
  template <typename>
  friend struct read_state_t;

  template <typename, typename>
  friend struct map_by_schema_t;

  template <typename ParentConfig>
  constexpr read_state_t(const read_state_t<ParentConfig> &parent)
    : config(parent.config), error_count(parent.error_count),
      warning_count(parent.warning_count), map_node(parent.map_node) {}

  constexpr read_state_t(const typename StateConfig::config_type &options,
    std::size_t errors, std::size_t warnings,
    const typename StateConfig::map_node_type &mapping)
    : config(options), error_count(errors), warning_count(warnings),
      map_node(mapping) {}
};

/** Serialization policy, local mapping selection, and schema position.
 * State and node handles borrow the active builder call and must not escape it.
 * Root states are created by the tree builders; custom mappings copy the
 * supplied state or derive children. BorrowStrings permits borrowing
 * source-backed strings; temporary owning conversions are still copied.
 * The mapping selector is borrowed. Mapping callbacks take a generic State
 * constrained by is_write_state, preserving the child path and local selector
 * when forwarding it. enclosing_root and enclosing_record follow the same
 * rules as read_state_t. Path-aware selectors
 * take State by value for C++17/20 constant evaluation. */
template <typename StateConfig = detail::write_state_config<>>
struct write_state_t {
  using enclosing_root = typename StateConfig::enclosing_root;
  using enclosing_record = typename StateConfig::enclosing_record;

  static constexpr bool borrow_strings = StateConfig::borrow_strings;
  const typename StateConfig::map_node_type &map_node;

  /** Destination node, valid only during the active builder call. */
  ::ryml::NodeRef node() const { return {this->tree, this->node_id}; }

  /** Copy this traversal context for another node in its output tree.
   * Path and depth are unchanged; use child(), field(), or element() for
   * a schema edge. @pre destination is a readable node in the same tree. */
  write_state_t at(::ryml::NodeRef destination) const {
    assert(destination.readable() && destination.tree() == this->tree);
    return {*this, destination.id()};
  }

  /** Tree depth, starting at zero for the root.
   * child(), field(), and element() increment it; optional unwrapping does not.
   * At the limit, these operations return depth_exceeded instead of a state. */
  static constexpr std::size_t depth() { return StateConfig::depth; }

  /** Maximum tree depth, defaulting to 10. */
  static constexpr std::size_t depth_limit() {
    return StateConfig::depth_limit;
  }

  /** Canonical schema path; see read_state_t::path. */
  static constexpr auto path()
    OMNI_CPP11_TRAILING_RETURN(typename StateConfig::path_type::text) {
    return StateConfig::path_type::string();
  }

  /** Child state from a string token: child(OMNI_META_STRING("address"))
   * appends .address. Every call appends; reflected-field identity is unknown.
   * Record sets enclosing_record; omitted, the current record is retained.
   * At depth_limit(), returns depth_exceeded instead. */
  template <typename Record = enclosing_record, typename Name>
  constexpr auto child(Name) const
    -> typename detail::state_children<write_state_t, StateConfig>::template
      child<Record, Name> {
    return typename detail::state_children<write_state_t, StateConfig>::template
      child<Record, Name>{*this};
  }

  /** Child state from reflected metadata: appends `.` then field.name().
   * Repeating the same field metadata reuses its first path, removing the
   * intervening suffix to keep recursive schema paths finite.
   * Record sets enclosing_record; omitted, the current record is retained.
   * At depth_limit(), returns depth_exceeded instead. */
  template <typename Record = enclosing_record, typename Field>
  constexpr auto field(Field) const
    -> typename detail::state_children<write_state_t, StateConfig>::template
      field<Record, Field> {
    return typename detail::state_children<write_state_t, StateConfig>::template
      field<Record, Field>{*this};
  }

  /** Container-value state: root.items becomes root.items[].
   * Indices and mapping keys share []; enclosing_record is retained.
   * At depth_limit(), returns depth_exceeded instead. */
  constexpr auto element() const
    -> typename detail::state_children<write_state_t, StateConfig>::element {
    return typename detail::state_children<write_state_t, StateConfig>::element{
      *this};
  }

  // Rvalue propagation copies this borrowed traversal token as well.
  constexpr write_state_t(const write_state_t &) = default;

private:
  // Child states use other configurations and need constructor access.
  template <typename>
  friend struct write_state_t;

  template <typename>
  friend struct as_owning_tree_t;

  template <typename>
  friend struct as_ref_tree_t;

  template <typename ParentConfig>
  constexpr write_state_t(const write_state_t<ParentConfig> &parent)
    : map_node(parent.map_node), tree(parent.tree),
      node_id(parent.node_id) {}

  constexpr write_state_t(const typename StateConfig::map_node_type &mapping,
    ::ryml::Tree *tree, ::ryml::id_type node_id)
    : map_node(mapping), tree(tree), node_id(node_id) {}

  constexpr write_state_t(const write_state_t &parent, ::ryml::id_type node_id)
    : map_node(parent.map_node), tree(parent.tree), node_id(node_id) {}

  ::ryml::Tree *tree;
  ::ryml::id_type node_id;
};

/** Whether T is a read_state_t specialization, ignoring cv/ref qualifiers. */
template <typename T>
struct is_read_state:
    compat::bool_constant<traits::is_specialization_of<read_state_t, T>()> {};

/** Whether T is a write_state_t specialization, ignoring cv/ref qualifiers. */
template <typename T>
struct is_write_state:
    compat::bool_constant<traits::is_specialization_of<write_state_t, T>()> {};

// C++14 adds variable templates for trait values.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <typename T>
constexpr bool is_read_state_v = is_read_state<T>::value;

template <typename T>
constexpr bool is_write_state_v = is_write_state<T>::value;
#endif

// C++20 concepts constrain customization state without exposing its parameters.
#if defined(__cpp_concepts) && 201907L <= __cpp_concepts
template <typename T>
concept read_state_like = is_read_state<T>::value;

template <typename T>
concept write_state_like = is_write_state<T>::value;
#endif

} // namespace ryml
} // namespace omni

// REFACTORME: [high] Simplify traversal-state types and implementation.
