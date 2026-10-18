#include <omnirefl/serialization/ryml.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

// Keep this translation unit free of reflected records: these roots must not
// depend on metadata discovered through unrelated record serialization calls.
TEST(serialization_scalar_roots, writes_scalar_values_without_record_metadata) {
  EXPECT_EQ("42", omni::ryml::as_json(42));
  EXPECT_EQ("true", omni::ryml::as_json(true));
  EXPECT_EQ("1.5", omni::ryml::as_json(1.5));
  EXPECT_EQ(R"("text")", omni::ryml::as_json(std::string{"text"}));
  EXPECT_EQ(R"("")", omni::ryml::as_json(std::string{}));
}

TEST(serialization_scalar_roots, writes_string_array_without_record_metadata) {
  const auto parsed = omni::ryml::parse(omni::ryml::as_json(
    std::vector<std::string>{"first", "second"}));

  EXPECT_TRUE(parsed);
  if (!parsed)
    return;
  const auto root = parsed->crootref();
  EXPECT_TRUE(root.is_seq());
  if (!root.is_seq())
    return;
  EXPECT_EQ(2U, root.num_children());
  if (2U != root.num_children())
    return;
  EXPECT_EQ("first", root[0].val());
  EXPECT_EQ("second", root[1].val());
  EXPECT_TRUE(root[0].is_val_quoted());
  EXPECT_TRUE(root[1].is_val_quoted());
}

TEST(serialization_scalar_roots, writes_boolean_array_without_record_metadata) {
  const auto parsed = omni::ryml::parse(
    omni::ryml::as_json(std::vector<bool>{true, false}));

  EXPECT_TRUE(parsed);
  if (!parsed)
    return;
  const auto root = parsed->crootref();
  EXPECT_TRUE(root.is_seq());
  if (!root.is_seq())
    return;
  EXPECT_EQ(2U, root.num_children());
  if (2U != root.num_children())
    return;
  EXPECT_EQ("true", root[0].val());
  EXPECT_EQ("false", root[1].val());
  EXPECT_FALSE(root[0].is_val_quoted());
  EXPECT_FALSE(root[1].is_val_quoted());
}

TEST(serialization_scalar_roots, writes_empty_array_without_record_metadata) {
  const auto parsed =
    omni::ryml::parse(omni::ryml::as_json(std::vector<int>{}));

  EXPECT_TRUE(parsed);
  if (!parsed)
    return;
  EXPECT_TRUE(parsed->crootref().is_seq());
  EXPECT_EQ(0U, parsed->crootref().num_children());
}

} // namespace
