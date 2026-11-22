#pragma once

/** Parsing, node-shape checks, value conversions, and schema dispatch.
 * Raw map_node_to_* functions require validated input and do not select
 * custom type mappings. Schema dispatch definitions are supplied by ryml.hpp.
 */

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/traits.hpp>
#include <omnirefl/serialization/ryml/impl/depth.hpp>
#include <omnirefl/serialization/ryml/state.hpp>
#include <omnirefl/serialization/ryml/with_diagnostics.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <string>
#include <type_traits>
#include <vector>

namespace omni {
namespace ryml {
namespace detail {

template <typename T>
struct is_mapped_range;

} // namespace detail

namespace core {

/**
 * Non-throwing adapter for RapidYAML parsing errors. Parses text into an
 * owning tree and returns syntax errors through expected.
 *
```
parse :: (string, ParserOptions) -> expected<tree, string>
```
 *
 * ParserOptions are passed directly to ryml. Location tracking, if enabled,
 * ends with this call; the returned tree does not retain the parser.
 *
```cpp
#include <omnirefl/serialization/ryml.hpp>

// Supply a reflected person and an owning std::string source.
auto parsed = core::parse(std::move(source));
if (parsed) {
  auto result = map_by_schema(omni::type<person>, *parsed);
  // Before C++14: omni::type_t<person>{}.
  // Use result while parsed keeps its tree and character storage alive.
}
// Syntax errors use expected; mapping returns optional person and diagnostics.
``` */
inline compat::expected<::ryml::Tree, std::string> parse(std::string source,
  ::ryml::ParserOptions options = {});

/** The document path of a node, retaining the original object keys.
 * Sequence positions come from the node's parent. Invalid and root nodes
 * have an empty path. Construction is intended for diagnostics only.
 * @pre A valid node's tree and character storage remain alive. */
inline std::vector<issue::path_segment> node_path(::ryml::ConstNodeRef node);

/** An issue identifying a source node and its document path.
 * Paths are constructed only when an issue is reported. Invalid nodes have
 * no source ID or path, as with a syntax error before mapping begins.
 * Object-key views borrow the tree's character storage. */
inline issue make_issue(json::issue_code reason, json::value_kind expected,
  ::ryml::ConstNodeRef node, bool warning = false);

/** Parses true/false text, accepting lowercase, title case, and uppercase.
 * Other spellings return the unrecognized input view as the error; its
 * character storage must remain valid while that view is used. */
inline OMNI_CPP14_CONSTEXPR auto parse_boolean(compat::string_view text)
  -> compat::expected<bool, compat::string_view>;

/** Checks for an object node.
 *
```
expect_object_node :: node -> expected<node, issue>
```
 *
 * @pre node must belong to a valid tree. */
inline auto expect_object_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue>;

/** Checks for an array node.
 *
```
expect_array_node :: node -> expected<node, issue>
```
 *
 * @pre node must belong to a valid tree. */
inline auto expect_array_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue>;

/** Checks for a scalar node, including null.
 *
```
expect_scalar_node :: node -> expected<node, issue>
```
 *
 * @pre node must belong to a valid tree. */
inline auto expect_scalar_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue>;

/** Checks for a value node: scalar, object, or array. Null is a scalar value.
 * Optional mapping checks the contained type separately for non-null input.
 *
```
expect_value_node :: node -> expected<node, issue>
```
 *
 * @pre node must belong to a valid tree. */
inline auto expect_value_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue>;

/** Checks for a string node, honoring quotes and explicit YAML string tags.
 * Plain null, Boolean, and numeric spellings are not strings.
 *
```
expect_string_node :: node -> expected<node, issue>
```
 *
 * @pre node must belong to a valid tree. */
inline auto expect_string_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue>;

/** Maps a scalar node's text to a char-based string-like value.
 * Custom string APIs are supported through \ref adapt::from_string.
 * Its callable object can be passed by value for C++11 composition.
 *
```
map_node_to_string_like :: (state, node, type<T>) -> with_diagnostics<T>
```
 *
 * @pre Caller validates the node shape. node must refer to a scalar in a
 * valid tree; only a debug assertion checks the scalar-node precondition. */
struct map_node_to_string_like_t:
    omni::ryml::detail::with_depth_exceeded_operator {
  using omni::ryml::detail::with_depth_exceeded_operator::operator();

  // REFACTORME: [medium] Consider replacing ConstNodeRef with value-level
  // input; node validation belongs to composition.
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<traits::is_string_like<T>,
      is_read_state<State>>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, omni::type_t<T>) const
    -> with_diagnostics_t<T, diagnostics>;
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_node_to_string_like_t map_node_to_string_like{};
#else
static constexpr const map_node_to_string_like_t &map_node_to_string_like =
  compat::detail::shared_state<map_node_to_string_like_t>::value;
#endif

/** Checks for unquoted Boolean text, including an explicit YAML Boolean tag.
 * @pre node must belong to a valid tree. */
inline auto expect_boolean_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue>;

/** Boolean scalar-to-value mapping.
 *
```
map_node_to_boolean :: (state, node, type<bool>) -> with_diagnostics<bool>
```
 *
 * @pre node refers to an unquoted true/false scalar in a valid tree.
 *
 * @details Uses \ref parse_boolean for conversion. The caller validates the
 * input; debug assertions check the precondition. */
struct map_node_to_boolean_t: omni::ryml::detail::with_depth_exceeded_operator {
  using omni::ryml::detail::with_depth_exceeded_operator::operator();

  // REFACTORME: [medium] Consider replacing ConstNodeRef with value-level
  // input; node validation belongs to composition.
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_same<bool, compat::remove_cv_t<T>>::value
      && !std::is_const<T>::value && is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, omni::type_t<T>) const
    -> with_diagnostics_t<T, diagnostics>;
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_node_to_boolean_t map_node_to_boolean{};
#else
static constexpr const map_node_to_boolean_t &map_node_to_boolean =
  compat::detail::shared_state<map_node_to_boolean_t>::value;
#endif

/** Checks for unquoted integer text.
 * Destination range is checked on conversion.
 * @pre node must belong to a valid tree. */
inline auto expect_integer_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue>;

/** Maps a scalar node's text to an integer value.
 * Reports overflow through mapping diagnostics.
 *
```
map_node_to_integral :: (state, node, type<T>) -> with_diagnostics<T>
```
 *
 * @pre Caller validates integer input in a valid tree; a debug assertion
 * checks this precondition. Destination range is checked during conversion. */
struct map_node_to_integral_t:
    omni::ryml::detail::with_depth_exceeded_operator {
  using omni::ryml::detail::with_depth_exceeded_operator::operator();

  // REFACTORME: [medium] Consider replacing ConstNodeRef with value-level
  // input; node validation belongs to composition.
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_integral<T>::value
      && !std::is_same<bool, compat::remove_cv_t<T>>::value
      && is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, omni::type_t<T>) const
    -> with_diagnostics_t<T, diagnostics>;
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_node_to_integral_t map_node_to_integral{};
#else
static constexpr const map_node_to_integral_t &map_node_to_integral =
  compat::detail::shared_state<map_node_to_integral_t>::value;
#endif

/** Checks for unquoted numeric text.
 * Destination range is checked on conversion.
 * @pre node must belong to a valid tree. */
inline auto expect_number_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue>;

/** Maps a scalar node's text to a real-number value.
 * Reports failed conversions through mapping diagnostics.
 *
```
map_node_to_floating_point :: (state, node, type<T>) -> with_diagnostics<T>
```
 *
 * @pre Caller validates numeric input in a valid tree; a debug assertion
 * checks this precondition. */
struct map_node_to_floating_point_t:
    omni::ryml::detail::with_depth_exceeded_operator {
  using omni::ryml::detail::with_depth_exceeded_operator::operator();

  // REFACTORME: [medium] Consider replacing ConstNodeRef with value-level
  // input; node validation belongs to composition.
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_floating_point<T>::value && is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, omni::type_t<T>) const
    -> with_diagnostics_t<T, diagnostics>;
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_node_to_floating_point_t map_node_to_floating_point{};
#else
static constexpr const map_node_to_floating_point_t &
  map_node_to_floating_point =
    compat::detail::shared_state<map_node_to_floating_point_t>::value;
#endif

/** Checks for a Boolean or numeric node.
 * @pre node must belong to a valid tree. */
inline auto expect_arithmetic_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue>;

/** Maps a scalar node's text to the requested arithmetic type.
 * Provides a common entry point for Boolean, integer, and real mappings.
 * Boolean destinations require a non-const type.
 *
```
map_node_to_arithmetic :: (state, node, type<T>) -> with_diagnostics<T>
```
 *
 * @pre Caller validates Boolean input for bool, integer input for other
 * integral types, or numeric input for floating-point types, in a valid tree.
 * Only debug assertions check these preconditions. */
struct map_node_to_arithmetic_t:
    omni::ryml::detail::with_depth_exceeded_operator {
  using omni::ryml::detail::with_depth_exceeded_operator::operator();

  // REFACTORME: [medium] Consider replacing ConstNodeRef with value-level
  // input; node validation belongs to composition.
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_arithmetic<T>::value && is_read_state<State>::value
      && (!std::is_const<T>::value
        || !std::is_same<bool, compat::remove_cv_t<T>>::value)))
  auto operator()(State state, ::ryml::ConstNodeRef node, omni::type_t<T>) const
    -> with_diagnostics_t<T, diagnostics>;
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_node_to_arithmetic_t map_node_to_arithmetic{};
#else
static constexpr const map_node_to_arithmetic_t &map_node_to_arithmetic =
  compat::detail::shared_state<map_node_to_arithmetic_t>::value;
#endif

/** Terminal read: reports the depth limit without selecting a mapping. */
template <typename T>
auto read_node(const depth_exceeded &stopped, ::ryml::ConstNodeRef node,
  omni::type_t<T> type) -> with_diagnostics_t<T> {
  return omni::ryml::detail::with_depth_exceeded_operator{}(
    stopped, node, type);
}

template <typename T>
auto read_node(const depth_exceeded &stopped, ::ryml::ConstNodeRef node,
  T *into)
  -> with_diagnostics_t<compat::monostate> {
  return omni::ryml::detail::with_depth_exceeded_operator{}(
    stopped, node, into);
}

/** Node-to-type mapping through a customization or its default.
 *
```
read_node :: (state, node, type<T>) -> with_diagnostics<T>
```
 *
 * Pointer-only mappings are adapted when T is default-constructible.
 * @pre Required metadata is available in reflected scope. */
OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
  (is_read_state<State>::value))
auto read_node(State context, ::ryml::ConstNodeRef node, omni::type_t<T> type)
  -> with_diagnostics_t<T, diagnostics>;

// REFACTORME: [high] Review responsibility for diagnostic accumulation and
// limit checks between this dispatcher and the node mappers.
/** Dispatch one node through the selected to_type callable with diagnostics.
 *
```
read_node :: (state, node, T*) -> with_diagnostics<monostate>
```
 *
 * Prefers its type<T> overload when the produced value can be assigned to
 * *into; otherwise uses its T* overload. A mapping error does not trigger
 * fallback to another overload. */
OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
  (is_read_state<State>::value))
auto read_node(State context, ::ryml::ConstNodeRef node, T *into)
  -> with_diagnostics_t<compat::monostate, diagnostics>;

/** Terminal write returns an invalid node without modifying the tree.
 * See the depth-overflow FIXME in impl/depth.hpp. */
template <typename T>
auto write_node(const depth_exceeded &stopped, const T &value)
  -> ::ryml::NodeRef {
  return omni::ryml::detail::with_depth_exceeded_operator{}(stopped, value);
}

// REFACTORME: [high] This selects and invokes a mapping via state.map_node.
// Reassess whether a separate writer dispatcher is needed.
/** Dispatch one value through its serialization customization or default.
 *
```
write_node :: (state, const T&) -> node
```
 */
OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
  (is_write_state<State>::value))
auto write_node(State context, const T &value) -> ::ryml::NodeRef;

/** (Ad hoc: see detail) Map Boolean, integer, or real JSON/YAML nodes
 * into corresponding fundamental C++ types.
 *
```
map_to_fundamental :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
```
 *
 * @details reflected_call requires a metadata-bearing target. This route
 * handles arithmetic destinations without reflection.
 * @pre node must be valid; destination pointers must be non-null. */
struct map_to_fundamental_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_arithmetic<T>::value && !std::is_const<T>::value
      && is_read_state<State>::value))
  auto operator()(State context,
    ::ryml::ConstNodeRef node,
    omni::type_t<T> type) const
    -> with_diagnostics_t<T, diagnostics>;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_arithmetic<T>::value && !std::is_const<T>::value
      && is_read_state<State>::value))
  auto operator()(State context, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_to_fundamental_t map_to_fundamental{};
#else
static constexpr const map_to_fundamental_t &map_to_fundamental =
  compat::detail::shared_state<map_to_fundamental_t>::value;
#endif

/** (Ad hoc: see detail) Map JSON/YAML nodes into string-like C++ types.
 *
```
map_to_string_like :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
```
 *
 * @details String-like destinations may be instrumentation-disabled. This
 * route keeps them outside reflected_call.
 * @pre node must be valid; destination pointers must be non-null. */
struct map_to_string_like_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (traits::is_string_like<T>::value && !std::is_const<T>::value
      && is_read_state<State>::value))
  auto operator()(State context,
    ::ryml::ConstNodeRef node,
    omni::type_t<T> type) const
    -> with_diagnostics_t<T, diagnostics>;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (traits::is_string_like<T>::value && !std::is_const<T>::value
      && is_read_state<State>::value))
  auto operator()(State context, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_to_string_like_t map_to_string_like{};
#else
static constexpr const map_to_string_like_t &map_to_string_like =
  compat::detail::shared_state<map_to_string_like_t>::value;
#endif

/** (Ad hoc: see detail) Map JSON/YAML nodes into C++ types requiring
 * reflected scope.
 *
```
map_to_reflected :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
```
 *
 * @details reflected_call needs a metadata-bearing type, not an arbitrary
 * compound destination. Range and optional wrappers are unwrapped to obtain
 * that reflection target while mapping retains the original destination.
 * @pre node must be valid; destination pointers must be non-null. */
struct map_to_reflected_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_object<T>::value && !std::is_const<T>::value
      && is_read_state<State>::value))
  auto operator()(State context,
    ::ryml::ConstNodeRef node,
    omni::type_t<T> type) const
    -> with_diagnostics_t<T, diagnostics>;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_object<T>::value && !std::is_const<T>::value
      && is_read_state<State>::value))
  auto operator()(State context, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_to_reflected_t map_to_reflected{};
#else
static constexpr const map_to_reflected_t &map_to_reflected =
  compat::detail::shared_state<map_to_reflected_t>::value;
#endif

/** (Ad hoc: see detail) Map JSON/YAML arrays or objects into C++ ranges.
 *
```
map_to_sequence :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
```
 *
 * @details reflected_call cannot use the compound range as its reflection
 * target. Discovery follows element or mapped-value types before establishing
 * scope, preserving the original range as the destination. Fundamental and
 * string-like elements require no reflected scope, including empty ranges.
 * @pre node must be valid; destination pointers must be non-null. */
struct map_to_sequence_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (omni::ryml::detail::is_mapped_range<T>::value && !std::is_const<T>::value
      && is_read_state<State>::value))
  auto operator()(State context,
    ::ryml::ConstNodeRef node,
    omni::type_t<T> type) const
    -> with_diagnostics_t<T, diagnostics>;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (omni::ryml::detail::is_mapped_range<T>::value && !std::is_const<T>::value
      && is_read_state<State>::value))
  auto operator()(State context, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_to_sequence_t map_to_sequence{};
#else
static constexpr const map_to_sequence_t &map_to_sequence =
  compat::detail::shared_state<map_to_sequence_t>::value;
#endif

/** (Ad hoc: see detail) Map JSON/YAML nodes into optional-like C++ types.
 *
```
map_to_optional :: {
  (state, node, type<T>) -> with_diagnostics<T>,
  (state, node, T*)      -> with_diagnostics<monostate>
}
```
 *
 * @details reflected_call requires sanitizing this compound destination to
 * its contained type. Discovery establishes scope only when needed and keeps
 * the optional as the mapping destination, including when it is empty.
 * @pre node must be valid; destination pointers must be non-null. */
struct map_to_optional_t {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (traits::is_optional<T>::value && !std::is_const<T>::value
      && is_read_state<State>::value))
  auto operator()(State context,
    ::ryml::ConstNodeRef node,
    omni::type_t<T> type) const
    -> with_diagnostics_t<T, diagnostics>;

  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (traits::is_optional<T>::value && !std::is_const<T>::value
      && is_read_state<State>::value))
  auto operator()(State context, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate, diagnostics>;
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr map_to_optional_t map_to_optional{};
#else
static constexpr const map_to_optional_t &map_to_optional =
  compat::detail::shared_state<map_to_optional_t>::value;
#endif

} // namespace core
} // namespace ryml
} // namespace omni

#include <omnirefl/serialization/ryml/impl/core.hpp>
