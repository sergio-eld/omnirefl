#include "json_match.hpp"

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

#if defined(__GNUC__) && !defined(__clang__)
// GCC warns about GTest conditionals inside unbraced assertion guards.
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdangling-else"
#endif

namespace {

// Each route has its own record specialization, so another route cannot
// supply the metadata it needs.
struct vector_map {};
struct map_vector {};
struct optional_map {};
struct map_optional {};
struct custom_key {};
struct empty_maps {};
struct invalid_map {};

template <typename Route>
struct record {
  int value;

  bool operator==(const record &other) const {
    return this->value == other.value;
  }
};

struct my_string {
  std::string text;

  bool operator<(const my_string &other) const {
    return this->text < other.text;
  }

  bool operator==(const my_string &other) const {
    return this->text == other.text;
  }
};

} // namespace

#if defined(OMNI_TOOL_RUN)
OMNI_SKIP_METADATA(my_string);
#endif

template <>
struct omni::traits::cp::as_string<my_string> {
  omni::compat::string_view operator()(const my_string &value) const {
    return {value.text.data(), value.text.size()};
  }
};

template <>
struct omni::traits::cp::assign_string<my_string> {
  void operator()(my_string *into, omni::compat::string_view text) const {
    into->text.assign(text.empty() ? "" : text.data(), text.size());
  }
};

template <>
struct omni::ryml::cp::to_type_t<unsigned short> {
  template <typename State>
  auto operator()(::ryml::ConstNodeRef node, unsigned short *into,
    State state) const
    -> omni::ryml::with_diagnostics_t<omni::compat::monostate> {
    return omni::ryml::to_integer(omni::type_t<unsigned short>{})(
      node.is_seq() && 1 == node.num_children() ? node[0] : node,
      into,
      state);
  }
};

// This string's custom reader also accepts a one-element scalar wrapper.
// That observable distinction checks that registration overrides the default.
// C++14 supports callable variable-template registration and generic lambdas.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <>
const auto omni::ryml::cp::to_type<my_string> =
  [](::ryml::ConstNodeRef node, my_string *into, auto state) {
    return omni::ryml::to_string_like(omni::type_t<my_string>{})(
      node.is_seq() && 1 == node.num_children() ? node[0] : node,
      into,
      state);
  };
#else
template <>
struct omni::ryml::cp::to_type_t<my_string> {
  template <typename State>
  auto operator()(::ryml::ConstNodeRef node, my_string *into, State state) const
    -> omni::ryml::with_diagnostics_t<omni::compat::monostate> {
    return omni::ryml::to_string_like(omni::type_t<my_string>{})(
      node.is_seq() && 1 == node.num_children() ? node[0] : node,
      into,
      state);
  }
};
#endif

namespace {

TEST(mapping_dispatch, fundamental_registration_applies_at_the_root) {
  const auto result = serialization_test::deserialize(
    omni::type_t<unsigned short>{}, "[7]");

  EXPECT_EQ(7, result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
}

TEST(mapping_dispatch, fundamental_registration_applies_inside_sequences) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::vector<unsigned short>>{}, "[[7], [42]]");
  const std::vector<unsigned short> expected{7, 42};

  EXPECT_EQ(expected, result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
}

TEST(mapping_dispatch, vector_of_maps_discovers_only_the_mapped_record) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::vector<std::map<std::string, record<vector_map>>>>{},
    R"([{"4": {"value": 8}, "15": {"value": 16}}, {}, {"23": {"value": 42}}])");

  const std::vector<std::map<std::string, record<vector_map>>> expected{
    {{"4", {8}}, {"15", {16}}},
    {},
    {{"23", {42}}},
  };

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_EQ(expected, *result.value);
}

TEST(mapping_dispatch, map_of_vectors_discovers_the_record) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::map<std::string, std::vector<record<map_vector>>>>{},
    R"({"west": [{"value": 4}, {"value": 8}], "empty": []})");

  const std::map<std::string, std::vector<record<map_vector>>> expected{
    {"west", {{4}, {8}}},
    {"empty", {}},
  };

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_EQ(expected, *result.value);
}

TEST(mapping_dispatch, optional_sequence_of_maps_discovers_the_record) {
  const auto result = serialization_test::deserialize(
    omni::type_t<omni::compat::optional<std::vector<std::map<std::string,
      record<optional_map>>>>>{},

    R"([{}, {"first": {"value": 15}}])");

  const std::vector<std::map<std::string, record<optional_map>>> expected{
    {},
    {{"first", {15}}},
  };

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_TRUE(*result.value);
    if (*result.value)
      EXPECT_EQ(expected, **result.value);
  }
}

TEST(mapping_dispatch, maps_of_optional_records_preserve_null_values) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::vector<std::map<std::string,
      omni::compat::optional<record<map_optional>>>>>{},

    R"([{"present": {"value": 23}, "absent": null}])");

  const std::vector<std::map<std::string,
    omni::compat::optional<record<map_optional>>>> expected{
    {
      {"present", record<map_optional>{23}},
      {"absent", omni::compat::nullopt},
    },
  };

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_EQ(expected, *result.value);
}

TEST(mapping_dispatch, empty_compounds_still_discover_the_record_type) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::vector<std::map<std::string, record<empty_maps>>>>{},
    "[]");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_TRUE(result.value->empty());
}

TEST(mapping_dispatch, nested_errors_retain_the_document_path) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::vector<std::map<std::string, record<invalid_map>>>>{},
    R"([{"west": {"value": "bad"}}])");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  for (const auto &entry : result.diagnostics.issues) {
    EXPECT_EQ(omni::json::issue_code::invalid_scalar, entry.reason);
    EXPECT_EQ("/0/west/value", omni::ryml::detail::render_path(entry.path));
  }
}

TEST(mapping_dispatch, disabled_string_registration_needs_no_reflected_scope) {
  const auto result = serialization_test::deserialize(
    omni::type_t<my_string>{}, R"(["hello"])");
  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_EQ("hello", result.value->text);
}

TEST(mapping_dispatch, disabled_strings_inside_sequences_need_no_reflection) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::vector<my_string>>{}, R"([["first"], ["second"]])");
  const std::vector<my_string> expected{{"first"}, {"second"}};

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_EQ(expected, *result.value);
}

TEST(mapping_dispatch, disabled_string_keys_do_not_hide_the_mapped_record) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::vector<std::map<my_string, record<custom_key>>>>{},
    R"([{"west": {"value": 42}}, {}])");

  const std::vector<std::map<my_string, record<custom_key>>> expected{
    {{{"west"}, {42}}},
    {},
  };

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_EQ(expected, *result.value);
}

} // namespace

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
