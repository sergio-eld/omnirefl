#include "json_match.hpp"

#include <forward_list>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

namespace {

struct explicit_ranges {
  template <typename State, typename T>
  auto operator()(State state, omni::type_t<T> type) const
    -> decltype(omni::ryml::reflected_scope::fn::mapping_from_type(
      state, type)) {
    return omni::ryml::reflected_scope::fn::mapping_from_type(state, type);
  }

  template <typename State, typename T>
  auto operator()(State, omni::type_t<std::vector<T>>) const
    -> omni::ryml::map_range_t<std::true_type> {
    return omni::ryml::map_range.tdm_explicit_nulls(
      omni::fn::default_value<std::true_type>{});
  }
};

TEST(explicit_nulls, preserves_empty_optional_array_positions) {
  const std::vector<omni::compat::optional<int>> source{
    omni::compat::nullopt, omni::compat::make_optional(7),
    omni::compat::nullopt};
  const auto writer = omni::ryml::serialize.tdm_map_node(
    [](omni::ryml::reflected_scope::mapping_from_type_t) {
      return explicit_ranges{};
    });

  EXPECT_TRUE(serialization_test::matches_json("[null, 7, null]",
    writer(source)));
}

struct measurement {
  omni::compat::optional<int> value;
  std::vector<omni::compat::optional<int>> samples;
};

struct select_null_policy {
  bool record;
  bool range;

  template <typename State, typename T>
  auto operator()(State state, omni::type_t<T> type) const
    -> decltype(omni::ryml::reflected_scope::fn::mapping_from_type(
      state, type)) {
    return omni::ryml::reflected_scope::fn::mapping_from_type(state, type);
  }

  template <typename State>
  auto operator()(State, omni::type_t<measurement>) const
    -> omni::ryml::reflected_scope::map_record_t<
      omni::mapping::record_strategy_t<>, bool> {
    return omni::ryml::reflected_scope::map_record.tdm_explicit_nulls(
      [this](std::false_type) { return this->record; });
  }

  template <typename State, typename T>
  auto operator()(State, omni::type_t<std::vector<T>>) const
    -> omni::ryml::map_range_t<bool> {
    return omni::ryml::map_range.tdm_explicit_nulls(
      [this](std::false_type) { return this->range; });
  }
};

// Shared omission policy and local overrides determine emitted null values.
struct null_policy_case {
  std::string name;
  bool omit_fields;
  bool omit_elements;
  bool force_record;
  bool force_range;
  std::string expected;
};

class null_policy: public ::testing::TestWithParam<null_policy_case> {};

TEST_P(null_policy, serializes_record) {
  const auto &parameter = GetParam();
  const measurement source{omni::compat::nullopt,
    {omni::compat::make_optional(1), omni::compat::nullopt,
      omni::compat::make_optional(2)}};
  const auto writer = omni::ryml::serialize
    .tdm_config([&parameter](omni::ryml::write_config options) {
      options.omit_null_fields = parameter.omit_fields;
      options.omit_null_elements = parameter.omit_elements;
      return options;
    })
    .tdm_map_node(
      [&parameter](omni::ryml::reflected_scope::mapping_from_type_t) {
        return select_null_policy{parameter.force_record,
          parameter.force_range};
      });

  EXPECT_TRUE(serialization_test::matches_json(parameter.expected,
    writer(source)));
}

INSTANTIATE_TEST_SUITE_P(omission_and_overrides, null_policy,
  ::testing::Values(
    null_policy_case{"defaults", false, true, false, false,
      R"({"value":null,"samples":[1,2]})"},
    null_policy_case{"omit_both", true, true, false, false,
      R"({"samples":[1,2]})"},
    null_policy_case{"keep_elements", true, false, false, false,
      R"({"samples":[1,null,2]})"},
    null_policy_case{"force_record", true, true, true, false,
      R"({"value":null,"samples":[1,2]})"},
    null_policy_case{"force_range", true, true, false, true,
      R"({"samples":[1,null,2]})"},
    null_policy_case{"force_both", true, true, true, true,
      R"({"value":null,"samples":[1,null,2]})"}),
  [](const ::testing::TestParamInfo<null_policy_case> &parameter) {
    return parameter.param.name;
  });

TEST(explicit_nulls, preserves_null_root) {
  const omni::compat::optional<int> source;
  const auto writer = omni::ryml::serialize.tdm_config(
    [](omni::ryml::write_config options) {
      options.omit_null_fields = true;
      options.omit_null_elements = true;
      return options;
    });

  EXPECT_TRUE(serialization_test::matches_json("null", writer(source)));
}

TEST(explicit_nulls, omits_empty_associative_values) {
  const std::map<std::string, omni::compat::optional<int>> source{
    {"empty", omni::compat::nullopt},
    {"value", omni::compat::make_optional(7)}};
  const auto writer = omni::ryml::serialize.tdm_config(
    [](omni::ryml::write_config options) {
      options.omit_null_fields = true;
      return options;
    });

  EXPECT_TRUE(serialization_test::matches_json(R"({"value":7})",
    writer(source)));
}

TEST(explicit_nulls, maps_unsized_range) {
  const std::forward_list<omni::compat::optional<int>> source{
    omni::compat::make_optional(1), omni::compat::nullopt,
    omni::compat::make_optional(2)};
  const auto writer = omni::ryml::serialize.tdm_config(
    [](omni::ryml::write_config options) {
      options.omit_null_elements = false;
      return options;
    });

  EXPECT_TRUE(serialization_test::matches_json("[1,null,2]", writer(source)));
}

TEST(explicit_nulls, preserves_nested_empty_optional_position) {
  const std::vector<omni::compat::optional<omni::compat::optional<int>>> source{
    omni::compat::make_optional(omni::compat::optional<int>{})};
  const auto writer = omni::ryml::serialize.tdm_map_node(
    [](omni::ryml::reflected_scope::mapping_from_type_t) {
      return explicit_ranges{};
    });

  EXPECT_TRUE(serialization_test::matches_json("[null]", writer(source)));
}

struct result_item {
  omni::compat::optional<int> value;
};

TEST(explicit_nulls, preserves_positions_without_forcing_record_fields) {
  const std::vector<omni::compat::optional<result_item>> source{
    omni::compat::nullopt,
    omni::compat::make_optional(result_item{omni::compat::nullopt})};
  const auto writer = omni::ryml::serialize
    .tdm_config([](omni::ryml::write_config options) {
      options.omit_null_fields = true;
      return options;
    })
    .tdm_map_node([](omni::ryml::reflected_scope::mapping_from_type_t) {
      return explicit_ranges{};
    });

  EXPECT_TRUE(serialization_test::matches_json("[null,{}]", writer(source)));
}

TEST(explicit_nulls, tree_builder_keeps_policy_when_changing_selector) {
  const std::vector<omni::compat::optional<int>> source{
    omni::compat::nullopt, omni::compat::make_optional(7)};
  const auto build = omni::ryml::as_owning_tree
    .tdm_config([](omni::ryml::write_config options) {
      options.omit_null_elements = false;
      return options;
    })
    .tdm_map_node(omni::fn::identity{});
  const auto tree = build(source);

  EXPECT_TRUE(serialization_test::matches_json("[null,7]",
    ::ryml::emitrs_json<std::string>(tree)));
}

TEST(explicit_nulls, format_update_keeps_null_policy) {
  const std::vector<omni::compat::optional<int>> source{
    omni::compat::nullopt, omni::compat::make_optional(7)};
  const auto writer = omni::ryml::serialize
    .tdm_config([](omni::ryml::write_config options) {
      options.omit_null_elements = false;
      return options;
    })
    .tdm_format([](::ryml::EmitType_e) { return ::ryml::EMIT_YAML; });

  EXPECT_TRUE(serialization_test::matches_json("[null,7]", writer(source)));
}

} // namespace
