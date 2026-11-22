#pragma once

/** Parsing, node-shape checks, value conversions, and schema dispatch.
 * parse_as_* functions require validated input and do not select
 * custom type mappings. map_node.hpp and map_type.hpp supply recursive mapping
 * dispatch; ryml.hpp supplies schema entry-point routing. */

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
namespace core {

/**
 * Non-throwing adapter for RapidYAML parsing errors. Parses text into an
 * owning tree and returns syntax errors through expected.
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
 * @pre node must belong to a valid tree. */
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

/** Parses scalar text as a char-based string-like value.
 * String customization uses \ref adapt::from_string.
 * @pre The caller validates a scalar node in a valid tree. */
OMNI_CONSTRAINED_TEMPLATE((typename T),
  (traits::is_string_like<T>::value))
auto parse_as_string_like(::ryml::ConstNodeRef node)
  -> compat::expected<T, issue>;

/** Checks for a Boolean or numeric node.
 * @pre node must belong to a valid tree. */
inline auto expect_arithmetic_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue>;

/** Parses scalar text as an arithmetic value.
 * @pre The caller validates a Boolean or numeric node matching T
 * in a valid tree. */
OMNI_CONSTRAINED_TEMPLATE((typename T),
  (std::is_arithmetic<T>::value))
auto parse_as_arithmetic(::ryml::ConstNodeRef node)
  -> compat::expected<compat::remove_cv_t<T>, issue>;

/** Checks for unquoted Boolean text, including an explicit YAML Boolean tag.
 * @pre node must belong to a valid tree. */
inline auto expect_boolean_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue>;

/** Parses scalar text as a Boolean value.
 * @pre The caller validates an unquoted Boolean node in a valid tree. */
inline auto parse_as_boolean(::ryml::ConstNodeRef node)
  -> compat::expected<bool, issue>;

/** Checks for unquoted integer text.
 * Destination range is checked on conversion.
 * @pre node must belong to a valid tree. */
inline auto expect_integer_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue>;

/** Parses scalar text as an integral value.
 * @pre The caller validates an integer node in a valid tree. */
OMNI_CONSTRAINED_TEMPLATE((typename T),
  (std::is_integral<T>::value
    && !std::is_same<bool, compat::remove_cv_t<T>>::value))
auto parse_as_integral(::ryml::ConstNodeRef node)
  -> compat::expected<compat::remove_cv_t<T>, issue>;

/** Checks for unquoted numeric text.
 * Destination range is checked on conversion.
 * @pre node must belong to a valid tree. */
inline auto expect_number_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue>;

/** Parses scalar text as a floating-point value.
 * @pre The caller validates a numeric node in a valid tree. */
OMNI_CONSTRAINED_TEMPLATE((typename T),
  (std::is_floating_point<T>::value))
auto parse_as_floating_point(::ryml::ConstNodeRef node)
  -> compat::expected<compat::remove_cv_t<T>, issue>;

// Ad hoc: read_node accesses State before invoking the selected mapping,
// so its terminal overloads must intercept the sentinel here. Remove these
// duplicate guards when the dispatch boundary can forward it to the mapper.
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

// REFACTORME: [high] Reassess this State-dependent dispatch boundary and
// its overlap with mapping selection, adapters, and diagnostic limits.
/** Node-to-type mapping through a customization or its default.
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
 * Prefers its type<T> overload when the produced value can be assigned to
 * *into; otherwise uses its T* overload. A mapping error does not trigger
 * fallback to another overload. */
OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
  (is_read_state<State>::value))
auto read_node(State context, ::ryml::ConstNodeRef node, T *into)
  -> with_diagnostics_t<compat::monostate, diagnostics>;

} // namespace core
} // namespace ryml
} // namespace omni

#include <omnirefl/serialization/ryml/impl/core.hpp>
