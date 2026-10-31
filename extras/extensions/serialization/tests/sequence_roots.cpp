#include "json_match.hpp"

#include <gtest/gtest.h>

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
  const auto result = omni::ryml::deserialize(
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
  const auto result = omni::ryml::deserialize(
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

} // namespace
