#include <omnirefl/serialization/ryml/core.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#if defined(__GNUC__) && !defined(__clang__)
// GCC warns about GTest's internal conditionals in unbraced assertion guards.
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdangling-else"
#endif

namespace {

// FIXME: [medium] [external] Remove this patch-specific check once the
// supported tl::optional release includes the constexpr-storage fix and
// tl_optional-constexpr.patch is no longer needed.
TEST(optional_compat, constructs_values_in_constant_expressions) {
  constexpr omni::compat::optional<std::uint16_t> empty{};
  constexpr omni::compat::optional<std::uint16_t> integer{42};

  static_assert(!empty.has_value(), "Default construction is empty");
  static_assert(*integer == 42, "Value construction preserves the integer");
}

TEST(mapping_core, returns_syntax_errors_without_throwing) {
  const auto result = omni::ryml::core::parse("{");
  EXPECT_FALSE(result);
  if (!result)
    EXPECT_FALSE(result.error().empty());
}

TEST(mapping_core, reports_escaped_keys_and_sequence_positions) {
  const auto tree = omni::ryml::core::parse(
    R"({"outer/name": [0, {"~reading": []}]})");

  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto node = tree->crootref()["outer/name"][1]["~reading"];
  const auto result = omni::ryml::core::expect_integer_node(node);

  EXPECT_FALSE(result);
  if (!result) {
    EXPECT_EQ(omni::json::issue_code::unexpected_kind, result.error().reason);
    EXPECT_EQ(omni::json::value_kind::sequence, result.error().actual);
    EXPECT_EQ(node.id(), result.error().node_id);
    EXPECT_EQ("/outer~1name/1/~0reading",
      omni::ryml::detail::render_path(result.error().path));
  }
}

TEST(mapping_core, constructs_warning_paths_from_the_source_node) {
  const auto tree = omni::ryml::core::parse(
    R"({"items": [{"legacy_name": 7}, {"legacy_name": 8}]})");

  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto node = tree->crootref()["items"][1]["legacy_name"];
  const auto warning = omni::ryml::core::make_issue(
    omni::json::issue_code::unknown_field,
    omni::json::value_kind::unknown, node, /*warning=*/true);

  EXPECT_TRUE(warning.warning);
  EXPECT_EQ(node.id(), warning.node_id);
  EXPECT_EQ("/items/1/legacy_name",
    omni::ryml::detail::render_path(warning.path));
}

TEST(mapping_core, renders_diagnostics_without_the_umbrella_header) {
  const auto tree = omni::ryml::core::parse(R"({"count": []})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto checked = omni::ryml::core::expect_integer_node(
    tree->crootref()["count"]);

  EXPECT_FALSE(checked);
  if (checked)
    return;

  const omni::ryml::diagnostics report{
    /*config=*/{},
    /*issues=*/{checked.error()},
    /*parse_error_message=*/{},
  };

  EXPECT_EQ("/count: expected integer, found array",
    omni::ryml::render_diagnostics(*tree, report));
}

} // namespace

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
