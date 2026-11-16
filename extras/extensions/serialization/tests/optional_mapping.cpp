#include "json_match.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace {
namespace optional_mapping {

// Keep a viable has_value() default to exercise conditional specialization.
// Dereference, reset(), and emplace() require adapters.
template <typename T>
struct nullable {
  using value_type = T;

  bool absent = true;
  T contents{};

  bool is_null() const { return this->absent; }
  bool has_value() const { return !this->absent; }
  const T &get() const { return this->contents; }
  void clear() { this->absent = true; }
  void replace(T value) {
    this->contents = std::move(value);
    this->absent = false;
  }
};

struct payload { int value; };

struct optional_payload {
  omni::compat::optional<int> value;

  bool operator==(const optional_payload &other) const {
    return this->value == other.value;
  }
};

} // namespace optional_mapping
} // namespace

namespace omni {
namespace traits {
namespace cp {

template <typename T>
struct has_value<T,
  compat::void_t<decltype(std::declval<const T &>().is_null())>> {
  bool operator()(const T &value) const {
    return !value.is_null();
  }
};

template <typename T>
struct get_value<optional_mapping::nullable<T>> {
  const T &operator()(const optional_mapping::nullable<T> &value) const {
    return value.get();
  }
};

template <typename T>
struct reset<optional_mapping::nullable<T>> {
  void operator()(optional_mapping::nullable<T> *into) const { into->clear(); }
};

template <typename T>
struct emplace<optional_mapping::nullable<T>> {
  void operator()(optional_mapping::nullable<T> *into, T &&value) const {
    into->replace(std::move(value));
  }
};

} // namespace cp
} // namespace traits
} // namespace omni

namespace {

struct optional_string_case {
  const char *name;
  const char *source;
  omni::compat::optional<std::string> expected;
};

class optional_strings: public testing::TestWithParam<optional_string_case> {};

TEST_P(optional_strings, distinguishes_null_from_string_values) {
  const auto &c = GetParam();
  const auto result = serialization_test::deserialize(
    omni::type_t<omni::compat::optional<std::string>>{}, c.source);

  EXPECT_TRUE(result.diagnostics.issues.empty());
  EXPECT_EQ(omni::compat::make_optional(c.expected), result.value);
}

INSTANTIATE_TEST_SUITE_P(inputs,
  optional_strings,
  testing::Values(
    optional_string_case{"null", "null", omni::compat::nullopt},
    optional_string_case{"quoted_null", R"("null")", std::string{"null"}},
    optional_string_case{
      "escaped_null", R"("\u006eull")", std::string{"null"},
    },

    optional_string_case{"empty_string", R"("")", std::string{}}),

  [](const testing::TestParamInfo<optional_string_case> &info) {
    return info.param.name;
  });

TEST(optional_mapping, distinguishes_null_from_an_empty_array) {
  const auto null = serialization_test::deserialize(
    omni::type_t<omni::compat::optional<std::vector<int>>>{}, "null");

  const auto empty = serialization_test::deserialize(
    omni::type_t<omni::compat::optional<std::vector<int>>>{}, "[]");

  EXPECT_TRUE(null.diagnostics.issues.empty());
  EXPECT_TRUE(empty.diagnostics.issues.empty());
  EXPECT_EQ(omni::compat::make_optional(
    omni::compat::optional<std::vector<int>>{}), null.value);

  EXPECT_EQ(omni::compat::make_optional(
    omni::compat::make_optional(std::vector<int>{})), empty.value);
}

TEST(optional_mapping, distinguishes_null_from_an_empty_record) {
  const auto null = serialization_test::deserialize(
    omni::type_t<omni::compat::optional<optional_mapping::optional_payload>>{},
    "null");

  const auto empty = serialization_test::deserialize(
    omni::type_t<omni::compat::optional<optional_mapping::optional_payload>>{},
    "{}");

  EXPECT_TRUE(null.diagnostics.issues.empty());
  EXPECT_TRUE(empty.diagnostics.issues.empty());
  EXPECT_EQ(omni::compat::make_optional(
    omni::compat::optional<optional_mapping::optional_payload>{}), null.value);

  EXPECT_EQ(omni::compat::make_optional(omni::compat::make_optional(
    optional_mapping::optional_payload{})), empty.value);
}

TEST(optional_mapping, keeps_false_as_an_engaged_optional) {
  const auto result = serialization_test::deserialize(
    omni::type_t<omni::compat::optional<bool>>{}, "false");

  EXPECT_TRUE(result.diagnostics.issues.empty());
  EXPECT_EQ(omni::compat::make_optional(omni::compat::make_optional(false)),
    result.value);
}

TEST(optional_mapping, reads_null_into_a_standard_optional) {
  const auto result = serialization_test::deserialize(
    omni::type_t<omni::compat::optional<int>>{}, "null");

  // A decoded empty optional is still a successful mapping result.
  const auto expected = omni::compat::make_optional(
    omni::compat::optional<int>{});

  EXPECT_EQ(expected, result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
}

TEST(optional_mapping, reads_null_elements_in_an_optional_vector) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::vector<omni::compat::optional<int>>>{},
    "[0, null, 7, null]");

  const std::vector<omni::compat::optional<int>> expected{
    0,
    omni::compat::nullopt,
    7,
    omni::compat::nullopt,
  };

  EXPECT_EQ(expected, result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
}

TEST(optional_mapping, reads_an_empty_optional_vector) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::vector<omni::compat::optional<int>>>{}, "[]");

  EXPECT_EQ((std::vector<omni::compat::optional<int>>{}), result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
}

TEST(optional_mapping, detects_adapted_operations_without_registration) {
  EXPECT_TRUE(omni::traits::is_optional<
    optional_mapping::nullable<int>>::value);
  EXPECT_TRUE(omni::traits::is_optional<
    omni::compat::optional<int>>::value);
  EXPECT_FALSE(omni::traits::is_optional<int *>::value);
  EXPECT_FALSE(omni::traits::is_optional<std::unique_ptr<int>>::value);
}

TEST(optional_mapping, discovers_and_maps_the_contained_record) {
  const auto result = serialization_test::deserialize(
    omni::type_t<optional_mapping::nullable<optional_mapping::payload>>{},
    R"({"value": 42})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_FALSE(result.value->is_null());
    EXPECT_EQ(42, result.value->get().value);
    EXPECT_EQ(R"({"value": 42})", omni::ryml::as_json(*result.value));
  }
}

TEST(optional_mapping, maps_null_as_a_disengaged_optional) {
  const auto result = serialization_test::deserialize(
    omni::type_t<optional_mapping::nullable<int>>{}, "null");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_TRUE(result.value->is_null());
    EXPECT_EQ("null", omni::ryml::as_json(*result.value));
  }
}

TEST(optional_mapping, omits_disengaged_elements_from_sequences) {
  optional_mapping::nullable<int> present{};
  present.replace(42);
  const std::vector<optional_mapping::nullable<int>> values{{}, present, {}};

  EXPECT_EQ("[42]", omni::ryml::as_json(values));
}

} // namespace
