#include "../json_match.hpp"

#include <gtest/gtest.h>

#include <string>

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

} // namespace
