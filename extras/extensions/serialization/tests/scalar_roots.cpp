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
    R"(["first", "second"])",
    omni::ryml::as_json(std::vector<std::string>{"first", "second"})));
}

TEST(serialization_scalar_roots, writes_boolean_array_without_record_metadata) {
  EXPECT_TRUE(serialization_test::matches_json(
    R"([true, false])", omni::ryml::as_json(std::vector<bool>{true, false})));
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

#if defined(__cpp_lib_string_view)
TEST(serialization_scalar_roots, writes_standard_string_views) {
  const std::string storage{"prefix null suffix"};
  const std::string_view view{storage.data() + 7, 4};

  EXPECT_EQ(R"("null")", omni::ryml::as_json(view));
  EXPECT_EQ(R"("")", omni::ryml::as_json(std::string_view{}));
}

TEST(serialization_scalar_roots, writes_standard_string_view_array) {
  EXPECT_TRUE(serialization_test::matches_json(
    R"(["first", "second"])",
    omni::ryml::as_json(std::vector<std::string_view>{"first", "second"})));
}
#endif

TEST(deserialization_scalar_roots, reads_scalars_without_record_metadata) {
  const auto integer = omni::ryml::deserialize(omni::type_t<int>{}, "42");
  EXPECT_TRUE(integer.value);
  if (integer.value) {
    EXPECT_EQ(42, *integer.value);
  }
  EXPECT_TRUE(integer.diagnostics.issues.empty());

  const auto boolean = omni::ryml::deserialize(omni::type_t<bool>{}, "true");
  EXPECT_TRUE(boolean.value);
  if (boolean.value) {
    EXPECT_TRUE(*boolean.value);
  }

  const auto real = omni::ryml::deserialize(omni::type_t<double>{}, "1.5");
  EXPECT_TRUE(real.value);
  if (real.value) {
    EXPECT_EQ(1.5, *real.value);
  }

  const auto text = omni::ryml::deserialize(
    omni::type_t<std::string>{}, R"("text")");

  EXPECT_TRUE(text.value);
  if (text.value) {
    EXPECT_EQ("text", *text.value);
  }
}

TEST(deserialization_scalar_roots, reports_invalid_root_values) {
  const auto result = omni::ryml::deserialize(omni::type_t<int>{}, R"("bad")");
  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  for (const auto &issue : result.diagnostics.issues) {
    EXPECT_EQ(omni::json::issue_code::invalid_scalar, issue.reason);
    EXPECT_EQ(omni::json::value_kind::integer, issue.expected);
    EXPECT_TRUE(issue.path.empty());
  }
}

TEST(deserialization_scalar_roots, reads_scalar_sequence_roots) {
  const auto numbers = omni::ryml::deserialize(
    omni::type_t<std::vector<int>>{}, "[7, 42]");

  const std::vector<int> expected_numbers{7, 42};
  EXPECT_TRUE(numbers.value);
  if (numbers.value) {
    EXPECT_EQ(expected_numbers, *numbers.value);
  }

  const auto booleans = omni::ryml::deserialize(
    omni::type_t<std::vector<bool>>{}, "[true, false]");

  const std::vector<bool> expected_booleans{true, false};
  EXPECT_TRUE(booleans.value);
  if (booleans.value) {
    EXPECT_EQ(expected_booleans, *booleans.value);
  }

  const auto nested = omni::ryml::deserialize(
    omni::type_t<std::vector<std::vector<int>>>{}, "[[7], [], [42]]");

  const std::vector<std::vector<int>> expected_nested{{7}, {}, {42}};
  EXPECT_TRUE(nested.value);
  if (nested.value) {
    EXPECT_EQ(expected_nested, *nested.value);
  }
}

TEST(deserialization_scalar_roots, reports_empty_tree_roots) {
  const auto scalar = omni::ryml::map_tree(omni::type_t<int>{}, ::ryml::Tree{});
  EXPECT_FALSE(scalar.value);
  EXPECT_EQ(1U, scalar.diagnostics.issues.size());
  for (const auto &issue : scalar.diagnostics.issues) {
    EXPECT_EQ(omni::json::issue_code::unexpected_kind, issue.reason);
    EXPECT_EQ(omni::json::value_kind::integer, issue.expected);
  }

  const auto sequence = omni::ryml::map_tree(
    omni::type_t<std::vector<int>>{}, ::ryml::Tree{});

  EXPECT_FALSE(sequence.value);
  EXPECT_EQ(1U, sequence.diagnostics.issues.size());
  for (const auto &issue : sequence.diagnostics.issues) {
    EXPECT_EQ(omni::json::issue_code::unexpected_kind, issue.reason);
    EXPECT_EQ(omni::json::value_kind::sequence, issue.expected);
  }
}

TEST(deserialization_scalar_roots, reports_sequence_element_paths) {
  const auto load = omni::ryml::deserialize.transform_settings(
    [](omni::ryml::settings s) {
      s.error_policy.error_limit = 1;
      return s;
    });

  const auto result = load(
    omni::type_t<std::vector<int>>{}, R"(["bad", "also bad", 42])");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(2U, result.diagnostics.issues.size());
  EXPECT_EQ("/0: \"bad\" is not an integer\n/1: \"also bad\" is not an integer",
    omni::ryml::render_diagnostics(result.diagnostics));
}

} // namespace
