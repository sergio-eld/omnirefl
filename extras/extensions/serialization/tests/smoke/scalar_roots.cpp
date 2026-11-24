#include "../json_match.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

// These roots must work without metadata discovered through a record call.
TEST(serialization_scalar_roots, writes_scalar_values_without_record_metadata) {
  EXPECT_EQ("42", omni::ryml::as_json(42));
  EXPECT_EQ("true", omni::ryml::as_json(true));
  EXPECT_EQ("1.5", omni::ryml::as_json(1.5));
  EXPECT_EQ(R"("text")", omni::ryml::as_json(std::string{"text"}));
  EXPECT_EQ(R"("")", omni::ryml::as_json(std::string{}));
}

TEST(deserialization_scalar_roots, reads_scalars_without_record_metadata) {
  const auto integer = serialization_test::deserialize(omni::type_t<int>{}, "42");
  EXPECT_TRUE(integer.value);
  if (integer.value) {
    EXPECT_EQ(42, *integer.value);
  }
  EXPECT_TRUE(integer.diagnostics.issues.empty());

  const auto boolean = serialization_test::deserialize(omni::type_t<bool>{}, "true");
  EXPECT_TRUE(boolean.value);
  if (boolean.value) {
    EXPECT_TRUE(*boolean.value);
  }

  const auto real = serialization_test::deserialize(omni::type_t<double>{}, "1.5");
  EXPECT_TRUE(real.value);
  if (real.value) {
    EXPECT_EQ(1.5, *real.value);
  }

  const auto text = serialization_test::deserialize(
    omni::type_t<std::string>{}, R"("text")");

  EXPECT_TRUE(text.value);
  if (text.value) {
    EXPECT_EQ("text", *text.value);
  }
}

TEST(diagnostics_transform, renders_by_default) {
  const auto valid = omni::ryml::deserialize(omni::type_t<int>{}, "7");
  const auto invalid = omni::ryml::deserialize(omni::type_t<int>{}, "bad");

  EXPECT_TRUE(valid.value);
  EXPECT_EQ("", valid.diagnostics);
  EXPECT_FALSE(invalid.value);
  EXPECT_EQ("<root>: expected integer, found scalar", invalid.diagnostics);
}

TEST(inplace_deserialization, updates_a_scalar) {
  int into = 7;
  const auto result = omni::ryml::deserialize(&into, "42");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  EXPECT_EQ(42, into);
}

TEST(serialization_scalar_roots, writes_string_array_without_record_metadata) {
  EXPECT_TRUE(serialization_test::matches_json(
    R"(["first", "second"])",
    omni::ryml::as_json(std::vector<std::string>{"first", "second"})));
}

TEST(deserialization_scalar_roots, reads_scalar_sequence_roots) {
  const auto numbers = serialization_test::deserialize(
    omni::type_t<std::vector<int>>{}, "[7, 42]");

  const std::vector<int> expected_numbers{7, 42};
  EXPECT_TRUE(numbers.value);
  if (numbers.value) {
    EXPECT_EQ(expected_numbers, *numbers.value);
  }
}

} // namespace
