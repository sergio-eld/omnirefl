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

TEST(mapping_core, parses_and_converts_without_reflection_or_composition) {
  const auto tree = omni::ryml::core::parse(
    R"({"flag": true, "count": 42, "ratio": 2.5, "text": "reading"})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const omni::ryml::config_t<> options{};
  const omni::ryml::read_state_t<> state{options, 0, 0};
  const auto root = tree->crootref();
  const auto boolean = omni::ryml::core::map_node_to_boolean(state,
    root["flag"],
    omni::type_t<bool>{});

  const auto integer = omni::ryml::core::map_node_to_integral(state,
    root["count"],
    omni::type_t<std::int16_t>{});

  const auto real = omni::ryml::core::map_node_to_floating_point(state,
    root["ratio"],
    omni::type_t<float>{});

  const auto arithmetic = omni::ryml::core::map_node_to_arithmetic(state,
    root["count"],
    omni::type_t<std::int32_t>{});

  const auto text = omni::ryml::core::map_node_to_string_like(state,
    root["text"],
    omni::type_t<std::string>{});

  EXPECT_EQ(true, boolean.value);
  EXPECT_EQ(42, integer.value);
  EXPECT_EQ(2.5F, real.value);
  EXPECT_EQ(42, arithmetic.value);
  EXPECT_EQ(std::string{"reading"}, text.value);
  EXPECT_TRUE(boolean.diagnostics.issues.empty());
  EXPECT_TRUE(integer.diagnostics.issues.empty());
  EXPECT_TRUE(real.diagnostics.issues.empty());
  EXPECT_TRUE(arithmetic.diagnostics.issues.empty());
  EXPECT_TRUE(text.diagnostics.issues.empty());
}

TEST(mapping_core, returns_syntax_errors_without_throwing) {
  const auto result = omni::ryml::core::parse("{");
  EXPECT_FALSE(result);
  if (!result)
    EXPECT_FALSE(result.error().empty());
}

TEST(mapping_core, reports_overflow_at_the_nodes_document_path) {
  const auto tree = omni::ryml::core::parse(R"({"count": 128})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const omni::ryml::config_t<> options{};
  const omni::ryml::read_state_t<> root{options, 0, 0};
  const auto node = tree->crootref()["count"];
  const auto result = omni::ryml::core::map_node_to_integral(
    root, node, omni::type_t<std::int8_t>{});

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (result.diagnostics.issues.size() == 1) {
    EXPECT_EQ(omni::json::issue_code::out_of_range,
      result.diagnostics.issues[0].reason);

    EXPECT_EQ("/count", omni::ryml::detail::render_path(
      result.diagnostics.issues[0].path));
  }
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
