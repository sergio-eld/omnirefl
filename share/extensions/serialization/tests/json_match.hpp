#pragma once

#include <omnirefl/serialization/ryml.hpp>
#include <gtest/gtest.h>

#include <string>

namespace serialization_test {

// Compare structure, scalar spelling and quoting, ignoring map key order and
// formatting. Quoting distinguishes JSON strings from numbers and booleans.
inline testing::AssertionResult match_json_nodes(::ryml::ConstNodeRef expected,
  ::ryml::ConstNodeRef actual, const std::string &path) {
  if (expected.is_map() != actual.is_map()
    || expected.is_seq() != actual.is_seq()
    || expected.has_val() != actual.has_val()
    || expected.num_children() != actual.num_children()) {
    return testing::AssertionFailure() << path << ": different node shapes";
  }

  if (expected.is_map() || expected.is_seq()) {
    std::size_t index = 0;
    for (const auto child : expected.children()) {
      const auto segment = expected.is_map()
        ? omni::ryml::detail::to_string(child.key()) : std::to_string(index);
      if (expected.is_map() && !actual.has_child(child.key()))
        return testing::AssertionFailure() << path << "/" << segment
          << ": missing field";

      const auto result = match_json_nodes(child,
        expected.is_map() ? actual[child.key()] : actual[index],
        path + "/" + segment);
      if (!result)
        return result;
      ++index;
    }
    return testing::AssertionSuccess();
  }

  if (expected.has_val() && (expected.val() != actual.val()
    || expected.is_val_quoted() != actual.is_val_quoted())) {
    return testing::AssertionFailure() << path << ": expected "
      << (expected.is_val_quoted() ? "quoted " : "unquoted ")
      << omni::ryml::detail::to_string(expected.val()) << ", got "
      << (actual.is_val_quoted() ? "quoted " : "unquoted ")
      << omni::ryml::detail::to_string(actual.val());
  }
  return testing::AssertionSuccess();
}

// Compare an already-parsed node against a readable JSON or YAML document.
inline testing::AssertionResult matches_node(std::string expected,
  ::ryml::ConstNodeRef actual) {
  const auto expected_tree = omni::ryml::parse(std::move(expected));
  if (!expected_tree)
    return testing::AssertionFailure() << "Invalid expected document: "
      << expected_tree.error();
  return match_json_nodes(expected_tree->crootref(), actual, "");
}

// Actual output may be JSON or YAML; the expected document is written as JSON.
inline testing::AssertionResult matches_json(std::string expected,
  std::string actual) {
  const auto actual_tree = omni::ryml::parse(std::move(actual));
  if (!actual_tree)
    return testing::AssertionFailure() << "Invalid output: "
      << actual_tree.error();
  return matches_node(std::move(expected), actual_tree->crootref());
}

} // namespace serialization_test
