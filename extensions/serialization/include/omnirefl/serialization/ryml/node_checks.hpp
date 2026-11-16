#pragma once

/** Node-only checks for JSON/YAML mapping compositions. */

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/serialization/ryml/impl/shared.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <utility>

namespace omni {
namespace ryml {
namespace core {

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

// Node-only checks have no traversal state; composition supplies the path
// and diagnostic policies when the check is used during mapping.
inline auto node_check(::ryml::ConstNodeRef node, bool matches,
  json::value_kind expected) -> with_diagnostics_t<compat::monostate> {
  if (matches)
    return {compat::monostate{}, {}};

  return {
    /*value=*/compat::nullopt,
    /*diagnostics=*/{
      /*config=*/{},
      /*issues=*/{omni::ryml::detail::make_issue(
        json::issue_code::unexpected_kind, expected, node, {})},
      /*parse_error_message=*/{},
    },
  };
}

} // namespace detail

/** Checks for an object node.
 * @pre node must belong to a valid tree.
 *
```text
expect_object_node :: node -> with_diagnostics<monostate>
```
 */
inline auto expect_object_node(::ryml::ConstNodeRef node)
  -> with_diagnostics_t<compat::monostate> {
  return detail::node_check(node, node.is_map(), json::value_kind::mapping);
}

/** Checks for an array node.
 * @pre node must belong to a valid tree. */
inline auto expect_array_node(::ryml::ConstNodeRef node)
  -> with_diagnostics_t<compat::monostate> {
  return detail::node_check(node, node.is_seq(), json::value_kind::sequence);
}

/** Checks for a scalar node, including null.
 * @pre node must belong to a valid tree. */
inline auto expect_scalar_node(::ryml::ConstNodeRef node)
  -> with_diagnostics_t<compat::monostate> {
  return detail::node_check(node, node.has_val() && !node.is_container(),
    json::value_kind::scalar);
}

/** Checks for a string node, honoring quotes and explicit YAML string tags.
 * Plain null, Boolean, and numeric spellings are not strings.
 * @pre node must belong to a valid tree. */
inline auto expect_string_node(::ryml::ConstNodeRef node)
  -> with_diagnostics_t<compat::monostate> {
  const bool matches = node.has_val() && !node.is_container()
    && (node.has_val_tag()
      ? ::ryml::TAG_STR == ::ryml::to_tag(node.val_tag())
      : node.is_val_quoted()
        || (!node.val_is_null() && !node.val().is_number()
          && !parse_boolean({node.val().str, node.val().len})));

  return detail::node_check(node, matches, json::value_kind::string);
}

/** Checks for unquoted Boolean text, including an explicit YAML Boolean tag.
 * @pre node must belong to a valid tree. */
inline auto expect_boolean_node(::ryml::ConstNodeRef node)
  -> with_diagnostics_t<compat::monostate> {
  const bool matches = node.has_val() && !node.is_container()
    && !node.is_val_quoted()
    && (!node.has_val_tag()
      || ::ryml::TAG_BOOL == ::ryml::to_tag(node.val_tag()))
    && bool(parse_boolean({node.val().str, node.val().len}));

  return detail::node_check(node, matches, json::value_kind::boolean);
}

/** Checks for unquoted integer text.
 * Destination range is checked on conversion.
 * @pre node must belong to a valid tree. */
inline auto expect_integer_node(::ryml::ConstNodeRef node)
  -> with_diagnostics_t<compat::monostate> {
  const bool matches = node.has_val() && !node.is_container()
    && !node.is_val_quoted()
    && (!node.has_val_tag()
      || ::ryml::TAG_INT == ::ryml::to_tag(node.val_tag()))
    && node.val().is_integer();

  return detail::node_check(node, matches, json::value_kind::integer);
}

/** Checks for unquoted numeric text.
 * Destination range is checked on conversion.
 * @pre node must belong to a valid tree. */
inline auto expect_number_node(::ryml::ConstNodeRef node)
  -> with_diagnostics_t<compat::monostate> {
  const bool matches = node.has_val() && !node.is_container()
    && !node.is_val_quoted()
    && (!node.has_val_tag()
      || ::ryml::TAG_FLOAT == ::ryml::to_tag(node.val_tag())
      || ::ryml::TAG_INT == ::ryml::to_tag(node.val_tag()))
    && node.val().is_real();

  return detail::node_check(node, matches, json::value_kind::number);
}

/** Checks for a value node: scalar, object, or array. Null is a scalar value.
 * Optional mapping checks the contained type separately for non-null input.
 * @pre node must belong to a valid tree. */
inline auto expect_value_node(::ryml::ConstNodeRef node)
  -> with_diagnostics_t<compat::monostate> {
  return detail::node_check(node, node.has_val() || node.is_container(),
    json::value_kind::unknown);
}

} // namespace core
} // namespace ryml
} // namespace omni
