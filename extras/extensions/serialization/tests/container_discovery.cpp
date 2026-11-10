#include "json_match.hpp"

#include <gtest/gtest.h>

#include <deque>
#include <list>
#include <string>
#include <vector>

namespace {

// Each route uses its own model so another call cannot supply its metadata.
struct vector_record { int value; };
struct deque_record { int value; };
struct list_record { int value; };
struct nested_record { int value; };
enum class container_status { first, second };

struct my_int { int value; };

} // namespace

namespace omni {
namespace ryml {
namespace cp {

template <>
struct from_type<my_int> {
  void operator()(const my_int &value, ::ryml::NodeRef node,
    detail::mapping_context &diagnostics) const {
    core::write_node(value.value, node, diagnostics);
  }
};

template <>
struct to_type<my_int> {
  bool operator()(::ryml::ConstNodeRef node, my_int *into,
    detail::mapping_context &diagnostics) const {
    return core::read_node(node, &into->value, diagnostics);
  }
};

} // namespace cp
} // namespace ryml
} // namespace omni

namespace {

template <typename T>
class container_discovery: public testing::Test {};

using record_containers = testing::Types<std::vector<vector_record>,
  std::deque<deque_record>, std::list<list_record>>;
TYPED_TEST_SUITE(container_discovery, record_containers);

TYPED_TEST(container_discovery, discovers_and_maps_record_elements) {
  const std::string document = R"([{"value": 7}, {"value": 42}])";
  const auto result = omni::ryml::deserialize(
    omni::type_t<TypeParam>{}, document);
  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_EQ(2U, result.value->size());
    if (2U == result.value->size()) {
      EXPECT_EQ(7, result.value->front().value);
      EXPECT_EQ(42, result.value->back().value);
    }
    EXPECT_TRUE(serialization_test::matches_json(
      document, omni::ryml::as_json(*result.value)));
  }
}

TYPED_TEST(container_discovery, supports_empty_record_containers) {
  const auto result = omni::ryml::deserialize(omni::type_t<TypeParam>{}, "[]");
  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_TRUE(result.value->empty());
    EXPECT_TRUE(serialization_test::matches_json(
      "[]", omni::ryml::as_json(*result.value)));
  }
}

TEST(container_discovery, follows_nested_container_protocols) {
  const std::string document = R"([[{"value": 7}], [], [{"value": 42}]])";
  const auto result = omni::ryml::deserialize(
    omni::type_t<std::deque<std::list<nested_record>>>{}, document);
  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_TRUE(serialization_test::matches_json(
      document, omni::ryml::as_json(*result.value)));
  }
}

TEST(container_discovery, deserializes_three_nested_integer_vectors) {
  const auto result = omni::ryml::deserialize(
    omni::type_t<std::vector<std::vector<std::vector<int>>>>{},
    "[[[7, 42], []], [], [[0]]]");
  const std::vector<std::vector<std::vector<int>>> expected{
    {{7, 42}, {}},
    {},
    {{0}},
  };
  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_EQ(expected, *result.value);
  }
}

// No push_back: default insertion must retain the emplace_back protocol.
struct emplace_only {
  using value_type = int;
  std::vector<int> values;

  decltype(values)::const_iterator begin() const { return values.begin(); }
  decltype(values)::const_iterator end() const { return values.end(); }
  void clear() { values.clear(); }
  void emplace_back(int value) { values.emplace_back(value); }
};

TEST(container_discovery, inserts_into_an_emplace_only_container) {
  const auto result = omni::ryml::deserialize(
    omni::type_t<emplace_only>{}, "[7, 42]");
  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_EQ((std::vector<int>{7, 42}), result.value->values);
  }
}

TEST(container_discovery, deserializes_three_nested_custom_scalar_vectors) {
  const std::string document = "[[[7, 42], []], [], [[0]]]";
  const auto result = omni::ryml::deserialize(
    omni::type_t<std::vector<std::vector<std::vector<my_int>>>>{}, document);
  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_TRUE(serialization_test::matches_json(
      document, omni::ryml::as_json(*result.value)));
  }
}

TEST(container_discovery, discovers_enum_elements) {
  const std::string document = R"(["first", "second"])";
  const auto result = omni::ryml::deserialize(
    omni::type_t<std::deque<container_status>>{}, document);
  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_EQ(2U, result.value->size());
    if (2U == result.value->size()) {
      EXPECT_EQ(container_status::first, result.value->front());
      EXPECT_EQ(container_status::second, result.value->back());
    }
    EXPECT_TRUE(serialization_test::matches_json(
      document, omni::ryml::as_json(*result.value)));
  }
}

} // namespace
