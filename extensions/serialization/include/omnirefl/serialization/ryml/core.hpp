#pragma once

/** Parsing, node-shape checks, and metadata-independent value conversions.
 * Raw map_node_to_* functions require validated input. These operations do
 * not select custom type mappings or require composition. */

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/traits.hpp>
#include <omnirefl/traits.hpp>
#include <omnirefl/serialization/ryml/impl/shared.hpp>
#include <omnirefl/serialization/ryml/state.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <algorithm>
#include <cassert>
#include <string>
#include <type_traits>
#include <utility>
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
inline std::vector<issue::path_segment> node_path(::ryml::ConstNodeRef node) {
  if (node.invalid())
    return {};

  // TODO: [high] Benchmark counting path segments first, then allocating the
  // exact vector size and filling it in root-to-leaf order. Compare against
  // this single-pass collection and reversal before implementing.
  std::vector<issue::path_segment> path;
  for (auto current = node; current.has_parent(); current = current.parent()) {
    const auto parent = current.parent();
    if (parent.is_map())
      path.push_back({current.key(), compat::nullopt});
    else if (parent.is_seq())
      path.push_back({{}, parent.child_pos(current)});
  }

  std::reverse(path.begin(), path.end());
  return path;
}

/** An issue identifying a source node and its document path.
 * Paths are constructed only when an issue is reported. Invalid nodes have
 * no source ID or path, as with a syntax error before mapping begins.
 * Object-key views borrow the tree's character storage. */
inline issue make_issue(json::issue_code reason, json::value_kind expected,
  ::ryml::ConstNodeRef node, bool warning = false) {
  // C++11 cannot aggregate-initialize issue because it has member defaults.
  issue result{};
  result.reason = reason;
  result.expected = expected;
  result.actual = omni::ryml::detail::node_kind(node);
  result.warning = warning;
  result.path = node_path(node);
  if (!node.invalid())
    result.node_id = node.id();

  return result;
}

/** Parses true/false text, accepting lowercase, title case, and uppercase.
 * Other spellings return the unrecognized input view as the error; its
 * character storage must remain valid while that view is used. */
inline OMNI_CPP14_CONSTEXPR auto parse_boolean(compat::string_view text)
  -> compat::expected<bool, compat::string_view> {
  if (text == "true" || text == "True" || text == "TRUE")
    return true;

  if (text == "false" || text == "False" || text == "FALSE")
    return false;

  return compat::unexpected<compat::string_view>{text};
}

namespace detail {

inline bool is_boolean_node(::ryml::ConstNodeRef node) {
  return node.has_val() && !node.is_container() && !node.is_val_quoted()
    && (!node.has_val_tag()
      || ::ryml::TAG_BOOL == ::ryml::to_tag(node.val_tag()))
    && bool(parse_boolean({node.val().str, node.val().len}));
}

inline bool is_number_node(::ryml::ConstNodeRef node) {
  return node.has_val() && !node.is_container() && !node.is_val_quoted()
    && (!node.has_val_tag()
      || ::ryml::TAG_FLOAT == ::ryml::to_tag(node.val_tag())
      || ::ryml::TAG_INT == ::ryml::to_tag(node.val_tag()))
    && node.val().is_real();
}

} // namespace detail

/** Checks for an object node.
 * @pre node must belong to a valid tree.
 *
```
expect_object_node :: node -> expected<node, issue>
```
 */
inline auto expect_object_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue> {
  if (node.is_map())
    return node;

  return compat::unexpected<issue>{make_issue(
    json::issue_code::unexpected_kind, json::value_kind::mapping, node)};
}

/** Checks for an array node.
 * @pre node must belong to a valid tree. */
inline auto expect_array_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue> {
  if (node.is_seq())
    return node;

  return compat::unexpected<issue>{make_issue(
    json::issue_code::unexpected_kind, json::value_kind::sequence, node)};
}

/** Checks for a scalar node, including null.
 * @pre node must belong to a valid tree. */
inline auto expect_scalar_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue> {
  if (node.has_val() && !node.is_container())
    return node;

  return compat::unexpected<issue>{make_issue(
    json::issue_code::unexpected_kind, json::value_kind::scalar, node)};
}

/** Checks for a value node: scalar, object, or array. Null is a scalar value.
 * Optional mapping checks the contained type separately for non-null input.
 * @pre node must belong to a valid tree. */
inline auto expect_value_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue> {
  if (node.has_val() || node.is_container())
    return node;

  return compat::unexpected<issue>{make_issue(
    json::issue_code::unexpected_kind, json::value_kind::unknown, node)};
}

/** Checks for a string node, honoring quotes and explicit YAML string tags.
 * Plain null, Boolean, and numeric spellings are not strings.
 * @pre node must belong to a valid tree. */
inline auto expect_string_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue> {
  const bool matches = node.has_val() && !node.is_container()
    && (node.has_val_tag()
      ? ::ryml::TAG_STR == ::ryml::to_tag(node.val_tag())
      : node.is_val_quoted()
        || (!node.val_is_null() && !node.val().is_number()
          && !parse_boolean({node.val().str, node.val().len})));

  if (matches)
    return node;

  return compat::unexpected<issue>{make_issue(
    json::issue_code::unexpected_kind, json::value_kind::string, node)};
}

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
    -> with_diagnostics_t<T, diagnostics> {
    assert(node.has_val() && !node.is_container());
    const auto scalar = node.val();
    using storage_type = compat::remove_cvref_t<decltype(adapt::as_string(
      std::declval<const T &>()))>;

    static_assert(std::is_same<char, typename storage_type::value_type>::value,
      "Ryml string mapping currently supports only char text.");

    // TODO: [medium] Define UTF-8 conversion for non-char strings and handling
    // of invalid UTF-8 through the shared operation customization points.
    // See the disabled wide-string case in tests/string_mapping.cpp.

    using view_type = compat::basic_string_view<
      typename storage_type::value_type, typename storage_type::traits_type>;

    return {
      /*value=*/adapt::from_string(omni::type_t<T>{},
        view_type{scalar.str, scalar.len}),
      /*diagnostics=*/{
        /*config=*/{state.config.error_policy, {}},
        /*issues=*/{},
        /*parse_error_message=*/{},
      },
    };
  }
};

constexpr map_node_to_string_like_t map_node_to_string_like{};

/** Checks for unquoted Boolean text, including an explicit YAML Boolean tag.
 * @pre node must belong to a valid tree. */
inline auto expect_boolean_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue> {
  if (detail::is_boolean_node(node))
    return node;

  return compat::unexpected<issue>{make_issue(
    json::issue_code::unexpected_kind, json::value_kind::boolean, node)};
}

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
    -> with_diagnostics_t<T, diagnostics> {
    assert(node.has_val() && !node.is_container() && !node.is_val_quoted());
    const auto scalar = node.val();
    const auto result = parse_boolean({scalar.str, scalar.len});
    assert(result.has_value());
    return {
      /*value=*/ *result,
      /*diagnostics=*/{
        /*config=*/{state.config.error_policy, {}},
        /*issues=*/{},
        /*parse_error_message=*/{},
      },
    };
  }
};

constexpr map_node_to_boolean_t map_node_to_boolean{};

/** Checks for unquoted integer text.
 * Destination range is checked on conversion.
 * @pre node must belong to a valid tree. */
inline auto expect_integer_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue> {
  const bool matches = node.has_val() && !node.is_container()
    && !node.is_val_quoted()
    && (!node.has_val_tag()
      || ::ryml::TAG_INT == ::ryml::to_tag(node.val_tag()))
    && node.val().is_integer();

  if (matches)
    return node;

  return compat::unexpected<issue>{make_issue(
    json::issue_code::unexpected_kind, json::value_kind::integer, node)};
}

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
    -> with_diagnostics_t<T, diagnostics> {
    assert(core::expect_integer_node(node));
    const auto expected = std::is_unsigned<T>::value
      ? json::value_kind::unsigned_integer : json::value_kind::integer;

    return omni::ryml::detail::read_scalar(std::move(state),
      node,
      omni::type_t<T>{},
      expected);
  }
};

constexpr map_node_to_integral_t map_node_to_integral{};

/** Checks for unquoted numeric text.
 * Destination range is checked on conversion.
 * @pre node must belong to a valid tree. */
inline auto expect_number_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue> {
  if (detail::is_number_node(node))
    return node;

  return compat::unexpected<issue>{make_issue(
    json::issue_code::unexpected_kind, json::value_kind::number, node)};
}

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
    -> with_diagnostics_t<T, diagnostics> {
    assert(core::expect_number_node(node));
    return omni::ryml::detail::read_scalar(std::move(state),
      node,
      omni::type_t<T>{},
      json::value_kind::number);
  }
};

constexpr map_node_to_floating_point_t map_node_to_floating_point{};

/** Checks for a Boolean or numeric node.
 * @pre node must belong to a valid tree. */
inline auto expect_arithmetic_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue> {
  if (detail::is_boolean_node(node) || detail::is_number_node(node))
    return node;

  return compat::unexpected<issue>{make_issue(
    json::issue_code::unexpected_kind, json::value_kind::scalar, node)};
}

/** Maps a scalar node's text to the requested arithmetic type.
 * Provides a common entry point for Boolean, integer, and real mappings.
 *
```
map_node_to_arithmetic :: (state, node, type<T>) -> with_diagnostics<T>
```
 *
 * @pre Caller validates the node shape. node must refer to a scalar in a
 * valid tree; only a debug assertion checks the scalar-node precondition. */
struct map_node_to_arithmetic_t {
  // REFACTORME: [medium] Consider replacing ConstNodeRef with value-level
  // input; node validation belongs to composition.
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (std::is_arithmetic<T>::value && is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node, omni::type_t<T>) const
    -> with_diagnostics_t<T, diagnostics> {
    assert(node.has_val() && !node.is_container());
    constexpr auto read = typename traits::select<
      traits::case_<std::is_same<bool, compat::remove_cv_t<T>>::value,
        core::map_node_to_boolean_t>,
      traits::case_<std::is_integral<T>::value, core::map_node_to_integral_t>,
      core::map_node_to_floating_point_t>::type{};

    return read(std::move(state), node, omni::type_t<T>{});
  }
};

constexpr map_node_to_arithmetic_t map_node_to_arithmetic{};

} // namespace core
} // namespace ryml
} // namespace omni

#include <omnirefl/serialization/ryml/impl/core.hpp>
