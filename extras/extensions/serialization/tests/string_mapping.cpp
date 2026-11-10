#include <omnirefl/serialization/ryml.hpp>

#include <gtest/gtest.h>

#include <string>
#include <type_traits>
#include <vector>

namespace {
namespace string_mapping {

struct alternate_traits: std::char_traits<char> {};

// Deliberately has no value_type, data(), size(), or (data, size) constructor.
template <typename Char, typename Traits = std::char_traits<Char>>
struct basic_text {
  std::basic_string<Char, Traits> contents;
};

} // namespace string_mapping
} // namespace

namespace omni {
namespace ryml {
namespace cp {

// Partial specializations cover the char-based template family, not one type.
template <typename Char, typename Traits>
struct read_string<string_mapping::basic_text<Char, Traits>,
  compat::enable_if_t<std::is_same<char, Char>::value>> {
  c4::csubstr operator()(
    const string_mapping::basic_text<Char, Traits> &value) const {
    return {value.contents.data(), value.contents.size()};
  }
};

template <typename Char, typename Traits>
struct assign_string<string_mapping::basic_text<Char, Traits>,
  compat::enable_if_t<std::is_same<char, Char>::value>> {
  void operator()(string_mapping::basic_text<Char, Traits> *into,
    c4::csubstr text) const {
    into->contents.assign(text.empty() ? "" : text.str, text.len);
  }
};

} // namespace cp
} // namespace ryml
} // namespace omni

namespace {

TEST(string_mapping, requires_both_operations) {
  EXPECT_TRUE(omni::ryml::is_string_like<
    string_mapping::basic_text<char>>::value);
  // Reading const strings is valid; assigning to them is not.
  EXPECT_FALSE(omni::ryml::is_string_like<const std::string>::value);
  EXPECT_FALSE(omni::ryml::is_string_like<std::vector<char>>::value);
  EXPECT_FALSE(omni::ryml::is_string_like<int>::value);
  EXPECT_FALSE(omni::ryml::is_string_like<int *>::value);
  EXPECT_FALSE(omni::ryml::is_string_like<
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

  const auto result = omni::ryml::deserialize(
    omni::type_t<TypeParam>{}, document);
  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_EQ(source.contents, result.value->contents);
  }
}

TYPED_TEST(custom_string_mapping, accepts_empty_text) {
  const auto result = omni::ryml::deserialize(
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
  const auto tree = omni::ryml::core::as_ref_tree(source);
  EXPECT_EQ(source.contents.data(), tree.crootref().val().str);
  EXPECT_EQ(c4::csubstr{"hello"}, tree.crootref().val());
}

TYPED_TEST(custom_string_mapping, owning_tree_retains_a_copy) {
  TypeParam source{{"hello"}};
  const auto tree = omni::ryml::core::as_owning_tree(source);
  source.contents[0] = 'x';
  EXPECT_EQ(c4::csubstr{"hello"}, tree.crootref().val());
}

} // namespace
