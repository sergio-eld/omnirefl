#include "json_match.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#if 201703L <= OMNI_CPLUSPLUS && defined(__has_include)
#  if __has_include(<string_view>)
#    include <string_view>
#  endif
#endif

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
  EXPECT_TRUE(serialization_test::matches_json(
    R"(["first","second"])",
    omni::ryml::as_json(std::vector<std::string>{"first", "second"})));
}

TEST(serialization_scalar_roots, writes_boolean_array_without_record_metadata) {
  EXPECT_TRUE(serialization_test::matches_json(
    R"([true,false])", omni::ryml::as_json(std::vector<bool>{true, false})));
}

TEST(serialization_scalar_roots, writes_empty_array_without_record_metadata) {
  EXPECT_TRUE(serialization_test::matches_json(
    R"([])", omni::ryml::as_json(std::vector<int>{})));
}

struct alternate_char_traits: std::char_traits<char> {};

TEST(serialization_scalar_roots, writes_basic_string_with_custom_traits) {
  const std::basic_string<char, alternate_char_traits> text{"12"};
  EXPECT_EQ(R"("12")", omni::ryml::as_json(text));
}

TEST(serialization_scalar_roots, writes_c4_string_views) {
  const std::string storage{"prefix true suffix"};
  const c4::csubstr view{storage.data() + 7, 4};

  EXPECT_EQ(R"("true")", omni::ryml::as_json(view));
  EXPECT_EQ(R"("")", omni::ryml::as_json(c4::csubstr{}));
}

#if defined(__cpp_lib_string_view) && 201606L <= __cpp_lib_string_view
TEST(serialization_scalar_roots, writes_standard_string_views) {
  const std::string storage{"prefix null suffix"};
  const std::string_view view{storage.data() + 7, 4};

  EXPECT_EQ(R"("null")", omni::ryml::as_json(view));
  EXPECT_EQ(R"("")", omni::ryml::as_json(std::string_view{}));
}

TEST(serialization_scalar_roots, writes_standard_string_view_array) {
  EXPECT_TRUE(serialization_test::matches_json(
    R"(["first","second"])",
    omni::ryml::as_json(std::vector<std::string_view>{"first", "second"})));
}
#endif

} // namespace
