#include "json_match.hpp"

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <type_traits>
#include <vector>

#if defined(__GNUC__) && !defined(__clang__)
// GCC warns about GTest's internal conditionals in unbraced assertion guards.
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdangling-else"
#endif

namespace {
namespace string_mapping {

struct alternate_traits: std::char_traits<char> {};

// Deliberately has no value_type, data(), size(), or (data, size) constructor.
template <typename Char, typename Traits = std::char_traits<Char>>
struct basic_text {
  std::basic_string<Char, Traits> contents;
};

struct converted_text { std::string contents; };

// Standard string construction without default construction or assignment.
struct constructed_text: std::string {
  using std::string::string;
  constructed_text() = delete;
  constructed_text(const constructed_text &) = delete;
  constructed_text(constructed_text &&) = default;
  constructed_text &operator=(const constructed_text &) = delete;
  constructed_text &operator=(constructed_text &&) = delete;
};

} // namespace string_mapping
} // namespace

namespace omni {
namespace traits {
namespace cp {

// Partial specializations cover the template family, not one concrete type.
template <typename Char, typename Traits>
struct as_string<string_mapping::basic_text<Char, Traits>,
  compat::enable_if_t<std::is_same<char, Char>::value>> {
  compat::basic_string_view<Char, Traits> operator()(
    const string_mapping::basic_text<Char, Traits> &value) const {
    return {value.contents.data(), value.contents.size()};
  }
};

template <typename Char, typename Traits>
struct from_string<string_mapping::basic_text<Char, Traits>,
  compat::enable_if_t<std::is_same<char, Char>::value>> {
  auto operator()(compat::basic_string_view<Char, Traits> text) const
    -> string_mapping::basic_text<Char, Traits> {
    return {adapt::from_string(omni::type_t<std::basic_string<Char, Traits>>{},
      text)};
  }
};

template <>
struct as_string<string_mapping::converted_text> {
  std::string operator()(const string_mapping::converted_text &value) const {
    return value.contents;
  }
};

template <>
struct from_string<string_mapping::converted_text> {
  string_mapping::converted_text operator()(compat::string_view text) const {
    return {adapt::from_string(omni::type_t<std::string>{}, text)};
  }
};

} // namespace cp
} // namespace traits
} // namespace omni

namespace {

TEST(string_mapping, requires_both_operations) {
  EXPECT_TRUE(omni::traits::is_string_like<
    string_mapping::basic_text<char>>::value);
  EXPECT_TRUE(omni::traits::is_string_like<const std::string>::value);
  EXPECT_FALSE(omni::traits::is_string_like<std::vector<char>>::value);
  EXPECT_FALSE(omni::traits::is_string_like<int>::value);
  EXPECT_FALSE(omni::traits::is_string_like<int *>::value);
  EXPECT_FALSE(omni::traits::is_string_like<
    string_mapping::basic_text<wchar_t>>::value);
}

template <typename T>
class custom_string_mapping: public testing::Test {};

using custom_strings = testing::Types<string_mapping::basic_text<char>,
  string_mapping::basic_text<char, string_mapping::alternate_traits>>;
TYPED_TEST_SUITE(custom_string_mapping, custom_strings);

TYPED_TEST(custom_string_mapping, round_trips_scalar_text) {
  const TypeParam source{{"hello"}};
  const auto document = omni::ryml::as_json(source);
  EXPECT_EQ(R"("hello")", document);

  const auto result = serialization_test::deserialize(
    omni::type_t<TypeParam>{}, document);
  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_EQ(source.contents, result.value->contents);
  }
}

TYPED_TEST(custom_string_mapping, accepts_empty_text) {
  const auto result = serialization_test::deserialize(
    omni::type_t<TypeParam>{}, R"("")");
  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_TRUE(result.value->contents.empty());
    EXPECT_EQ(R"("")", omni::ryml::as_json(*result.value));
  }
}

TYPED_TEST(custom_string_mapping, borrows_the_custom_string_storage) {
  const TypeParam source{{"hello"}};
  const auto tree = omni::ryml::as_ref_tree(source);
  EXPECT_EQ(source.contents.data(), tree.crootref().val().str);
  EXPECT_EQ(c4::csubstr{"hello"}, tree.crootref().val());
}

TYPED_TEST(custom_string_mapping, owning_tree_retains_a_copy) {
  TypeParam source{{"hello"}};
  const auto tree = omni::ryml::as_owning_tree(source);
  source.contents[0] = 'x';
  EXPECT_EQ(c4::csubstr{"hello"}, tree.crootref().val());
}

TEST(string_mapping, retains_owning_conversion_in_reference_tree) {
  string_mapping::converted_text source{"converted text"};
  const auto tree = omni::ryml::as_ref_tree(source);
  source.contents.clear();

  EXPECT_TRUE(tree.in_arena(tree.crootref().val()));
  EXPECT_EQ(c4::csubstr{"converted text"}, tree.crootref().val());
}

TEST(string_mapping, deserializes_through_owning_conversion_adapter) {
  const auto result = serialization_test::deserialize(
    omni::type_t<string_mapping::converted_text>{}, R"("converted text")");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_EQ("converted text", result.value->contents);
  }
}

TEST(string_mapping, constructs_without_default_construction_or_assignment) {
  const auto result = serialization_test::deserialize(
    omni::type_t<string_mapping::constructed_text>{}, R"("constructed text")");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_EQ(std::string{"constructed text"}, *result.value);
}

TEST(string_mapping, constructs_empty_text_from_a_null_view) {
  const auto owned = omni::adapt::from_string(omni::type_t<std::string>{},
    omni::compat::string_view{});

  const auto constructed = omni::adapt::from_string(
    omni::type_t<string_mapping::constructed_text>{},
    omni::compat::string_view{});

  const auto borrowed = omni::adapt::from_string(
    omni::type_t<omni::compat::string_view>{},
    omni::compat::string_view{});

  EXPECT_TRUE(owned.empty());
  EXPECT_TRUE(constructed.empty());
  EXPECT_TRUE(borrowed.empty());
}

TEST(string_mapping, constructs_non_default_string_keys) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::map<string_mapping::constructed_text, int>>{},
    R"({"": 3, "label": 7})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_EQ(2U, result.value->size());
    EXPECT_EQ(3, result.value->at(string_mapping::constructed_text{""}));
    EXPECT_EQ(7, result.value->at(string_mapping::constructed_text{"label"}));
  }
}

#if 0
// FIXME: [high] Wide strings are strings, but Ryml needs char text. This case
// cannot compile until encoding conversion can be supplied in both directions
// through operation specializations. Unsupported conversions need a focused
// diagnostic rather than constructor errors from the backend.
TEST(string_mapping, converts_wide_text) {
  const std::wstring source{L"hello"};
  EXPECT_EQ(R"("hello")", omni::ryml::as_json(source));

  const auto result = serialization_test::deserialize(
    omni::type_t<std::wstring>{}, R"("hello")");
  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_EQ(source, *result.value);
  }
}
#endif

} // namespace

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
