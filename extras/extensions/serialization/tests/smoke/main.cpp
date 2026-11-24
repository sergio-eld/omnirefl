#include "data.hpp"
#include "../json_match.hpp"

#include <gtest/gtest.h>

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

const auto strict_deserialize = serialization_test::deserialize;

TEST(deserialization, default_retains_value_and_diagnostics) {
  EXPECT_EQ(0, serialization_test::deserialize.config.error_policy.error_limit);
  EXPECT_FALSE((
    serialization_test::deserialize.config.error_policy.unknown_fields
    == omni::json::unknown_policy::allow));

  const auto result = serialization_test::deserialize(
    omni::type_t<serialization_data::payload>{},
    R"({"name": "Ada", "code": 815})");

  static_assert(std::is_same<decltype(result),
    const omni::ryml::with_diagnostics_t<serialization_data::payload,
      serialization_test::collected_diagnostics>>::value,
    "the default deserializer must retain a value and diagnostics");

  EXPECT_EQ("", result.diagnostics.message);
  EXPECT_TRUE(result.value);
  if (!result.value)
    return;

  EXPECT_EQ(815, result.value->code);
}

TEST(serialization_formats, emits_json_without_configuration) {
  const auto output =
    omni::ryml::as_json(serialization_data::payload{"Ada", 815});
  EXPECT_FALSE(output.empty());
  if (output.empty())
    return;

  EXPECT_EQ('{', output.front());

  const auto restored =
    strict_deserialize(omni::type_t<serialization_data::payload>{}, output);
  EXPECT_TRUE(restored.value);
  EXPECT_TRUE(restored.diagnostics.issues.empty());
  if (restored.value) {
    EXPECT_EQ("Ada", restored.value->name);
    EXPECT_EQ(815, restored.value->code);
  }
}

TEST(serialization_formats, emits_yaml_without_configuration) {
  const auto output =
    omni::ryml::as_yaml(serialization_data::payload{"Ada", 815});
  EXPECT_EQ(0U, output.find("name:"));

  const auto restored =
    strict_deserialize(omni::type_t<serialization_data::payload>{}, output);
  EXPECT_TRUE(restored.value);
  EXPECT_TRUE(restored.diagnostics.issues.empty());
  if (restored.value) {
    EXPECT_EQ("Ada", restored.value->name);
    EXPECT_EQ(815, restored.value->code);
  }
}

TEST(deserialization, maps_scalars_sequences_and_nested_records) {
  const auto result =
    strict_deserialize(omni::type_t<serialization_data::scalar_values>{},
      std::string{serialization_data::scalar_json});

  EXPECT_TRUE(result.value);
  if (!result.value)
    return;

  EXPECT_EQ("", result.diagnostics.message);
  EXPECT_TRUE(result.value->enabled);
  EXPECT_EQ(108U, result.value->retries);
  EXPECT_EQ(-42, result.value->delta);
  EXPECT_EQ((std::vector<int>{8, 15}), result.value->values);
  EXPECT_EQ("oceanic", result.value->label);
  EXPECT_EQ(2U, result.value->records.size());
  if (2U != result.value->records.size())
    return;
  EXPECT_EQ("oceanic", result.value->records[0].name);
  EXPECT_EQ(815, result.value->records[0].code);
  EXPECT_EQ("sunset", result.value->records[1].name);
  EXPECT_EQ(108, result.value->records[1].code);
}

TEST(deserialization, default_retains_syntax_errors) {
  const auto result =
    serialization_test::deserialize(omni::type_t<serialization_data::payload>{},
      "name: [unterminated");

  EXPECT_FALSE(result.value);
  if (result.value)
    return;

  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (1U == result.diagnostics.issues.size()) {
    EXPECT_EQ(omni::json::issue_code::parse_error,
      result.diagnostics.issues.front().reason);
  }
  EXPECT_FALSE((result.diagnostics.config.error_policy.unknown_fields
    == omni::json::unknown_policy::allow));
  EXPECT_FALSE(
    serialization_test::render_diagnostics(result.diagnostics).empty());
}

TEST(deserialization, reports_the_schema_scalar_expected_by_a_field) {
  const auto result =
    strict_deserialize(omni::type_t<serialization_data::nested_record>{},
      std::string{serialization_data::invalid_nested_integer_json});

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/data/code: expected integer, found scalar",
    result.diagnostics.message);
}

} // namespace
