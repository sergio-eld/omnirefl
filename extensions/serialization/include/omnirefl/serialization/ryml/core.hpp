#pragma once

/** Parsing, node-shape checks, and metadata-independent value conversions.
 * Raw map_node_to_* functions require validated input. These operations do
 * not select custom type mappings or require composition. */

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/traits.hpp>
#include <omnirefl/serialization/ryml/state.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <string>
#include <type_traits>
#include <vector>

namespace omni {
namespace ryml {
namespace core {

/**
 * Non-throwing adapter for RapidYAML parsing errors. Parses text into an
 * owning tree and returns syntax errors through expected.
 * ParserOptions are passed directly to ryml. Location tracking, if enabled,
 * ends with this call; the returned tree does not retain the parser.
 *
```cpp
#include <omnirefl/serialization/ryml.hpp>

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
 * @pre node must belong to a valid tree.
 *
```
expect_object_node :: node -> expected<node, issue>
```
 */
inline auto expect_object_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue>;

/** Checks for an array node.
 * @pre node must belong to a valid tree. */
inline auto expect_array_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue>;

/** Checks for a scalar node, including null.
 * @pre node must belong to a valid tree. */
inline auto expect_scalar_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue>;

/** Checks for a value node: scalar, object, or array. Null is a scalar value.
 * Optional mapping checks the contained type separately for non-null input.
 * @pre node must belong to a valid tree. */
inline auto expect_value_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue>;

/** Checks for a string node, honoring quotes and explicit YAML string tags.
 * Plain null, Boolean, and numeric spellings are not strings.
 * @pre node must belong to a valid tree. */
inline auto expect_string_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue>;

/** Maps a scalar node's text to a string-like value.
 * Custom string APIs are supported through \ref adapt::from_string.
 * Its callable object can be passed by value for C++11 composition.
 *
```
map_node_to_string_like :: (state, node, type<T>) -> with_diagnostics<T>
```
 *
 * @pre Caller validates the node shape. node must refer to a scalar in a
 * valid tree; only a debug assertion checks the scalar-node precondition. */
struct map_node_to_string_like_t {
  // REFACTORME: [medium] Consider replacing ConstNodeRef with value-level
  // input; node validation belongs to composition.
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (compat::conjunction<traits::is_string_like<T>,
      is_read_state<State>>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, omni::type_t<T>) const
    -> with_diagnostics_t<T, diagnostics>;
};

constexpr map_node_to_string_like_t map_node_to_string_like{};

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
struct map_node_to_boolean_t {
  // REFACTORME: [medium] Consider replacing ConstNodeRef with value-level
  // input; node validation belongs to composition.
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_same<bool, compat::remove_cv_t<T>>::value
      && !std::is_const<T>::value && is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, omni::type_t<T>) const
    -> with_diagnostics_t<T, diagnostics>;
};

constexpr map_node_to_boolean_t map_node_to_boolean{};

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
struct map_node_to_integral_t {
  // REFACTORME: [medium] Consider replacing ConstNodeRef with value-level
  // input; node validation belongs to composition.
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_integral<T>::value
      && !std::is_same<bool, compat::remove_cv_t<T>>::value
      && is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, omni::type_t<T>) const
    -> with_diagnostics_t<T, diagnostics>;
};

constexpr map_node_to_integral_t map_node_to_integral{};

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
struct map_node_to_floating_point_t {
  // REFACTORME: [medium] Consider replacing ConstNodeRef with value-level
  // input; node validation belongs to composition.
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_floating_point<T>::value && is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, omni::type_t<T>) const
    -> with_diagnostics_t<T, diagnostics>;
};

constexpr map_node_to_floating_point_t map_node_to_floating_point{};

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
struct map_node_to_arithmetic_t {
  // REFACTORME: [medium] Consider replacing ConstNodeRef with value-level
  // input; node validation belongs to composition.
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_arithmetic<T>::value && is_read_state<State>::value
      && (!std::is_const<T>::value
        || !std::is_same<bool, compat::remove_cv_t<T>>::value)))
  auto operator()(State state, ::ryml::ConstNodeRef node, omni::type_t<T>) const
    -> with_diagnostics_t<T, diagnostics>;
};

constexpr map_node_to_arithmetic_t map_node_to_arithmetic{};

} // namespace core
} // namespace ryml
} // namespace omni

#include <omnirefl/serialization/ryml/impl/core.hpp>
