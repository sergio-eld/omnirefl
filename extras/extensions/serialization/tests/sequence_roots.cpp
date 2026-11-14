#include "json_match.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

// This record appears only inside root containers, so a direct record call
// elsewhere in the translation unit cannot supply its reflection metadata.
struct pixel_sample {
  unsigned column;
  double alpha;
};

TEST(serialization_sequence_roots, discovers_nested_record_elements) {
  EXPECT_TRUE(serialization_test::matches_json(
    R"([[{"column": 7, "alpha": 0.5}]])", omni::ryml::as_json(
      std::vector<std::vector<pixel_sample>>{{pixel_sample{7, 0.5}}})));
}

TEST(serialization_sequence_roots, writes_empty_record_array) {
  EXPECT_TRUE(serialization_test::matches_json("[]",
    omni::ryml::as_json(std::vector<pixel_sample>{})));
}

TEST(deserialization_sequence_roots, discovers_nested_record_elements) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::vector<std::vector<pixel_sample>>>{},
    R"([[{"column": 7, "alpha": 0.5}], []])");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_EQ(2U, result.value->size());
    if (2U == result.value->size()) {
      EXPECT_TRUE((*result.value)[1].empty());
      EXPECT_EQ(1U, (*result.value)[0].size());
      for (const auto &sample : (*result.value)[0]) {
        EXPECT_EQ(7U, sample.column);
        EXPECT_EQ(0.5, sample.alpha);
      }
    }
  }
}

TEST(deserialization_sequence_roots, reads_empty_record_array) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::vector<pixel_sample>>{}, "[]");

  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_TRUE(result.value->empty());
  }
  EXPECT_TRUE(result.diagnostics.issues.empty());
}

// This type is discovered only through optional roots, not a direct record call.
struct optional_payload { int value; };

TEST(serialization_sequence_roots, discovers_record_elements_inside_optionals) {
  const std::vector<omni::compat::optional<optional_payload>> values{
    omni::compat::nullopt, optional_payload{7}, omni::compat::nullopt};

  EXPECT_TRUE(serialization_test::matches_json(
    R"([{"value": 7}])", omni::ryml::as_json(values)));
  EXPECT_TRUE(serialization_test::matches_json(
    R"([{"value": 7}])", omni::ryml::as_yaml(values)));
}

TEST(serialization_optional_roots, discovers_an_optional_record) {
  const omni::compat::optional<optional_payload> value{optional_payload{42}};
  EXPECT_TRUE(serialization_test::matches_json(
    R"({"value": 42})", omni::ryml::as_json(value)));
  EXPECT_TRUE(serialization_test::matches_json(
    "null", omni::ryml::as_yaml(omni::compat::optional<optional_payload>{})));
}

// Discover this record exclusively through the new tree-construction API.
struct tree_payload { std::string label; };

TEST(serialization_trees, discovers_records_inside_optional_sequences) {
  const std::vector<omni::compat::optional<tree_payload>> source{
    omni::compat::nullopt, tree_payload{"west"}};

  auto tree = omni::ryml::core::as_owning_tree(source);
  EXPECT_TRUE(serialization_test::matches_node(
    R"([{"label": "west"}])", tree.crootref()));

  tree.rootref()[0].remove_child("label");
  EXPECT_TRUE(serialization_test::matches_node("[{}]", tree.crootref()));

  // TODO: [medium] Mutate or destroy the nested source strings before checking
  // the owning tree, proving ownership through vector -> optional -> record.
  // TODO: [medium] Guard child access/removal after nonfatal shape checks so
  // malformed output reports a test failure instead of aborting the suite.
}

TEST(serialization_trees, borrows_record_string_members) {
  const tree_payload source{"west"};
  const auto tree = omni::ryml::core::as_ref_tree(source);
  EXPECT_TRUE(serialization_test::matches_node(
    R"({"label": "west"})", tree.crootref()));
  EXPECT_EQ(static_cast<const void *>(source.label.data()),
    static_cast<const void *>(tree.crootref()["label"].val().str));

  // TODO: [medium] Guard child access after the nonfatal shape check so
  // malformed output reports a test failure instead of aborting the suite.
}

} // namespace
