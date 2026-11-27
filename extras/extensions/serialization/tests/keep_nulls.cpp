#include "json_match.hpp"

#include <forward_list>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

namespace {

template <typename Keep = std::true_type>
struct range_null_policy {
  template <typename Path, typename T>
  auto operator()(Path path, omni::type_t<T> type) const
    -> decltype(omni::ryml::reflected_scope::fn::mapping_from_type(
      path, type)) {
    return omni::ryml::reflected_scope::fn::mapping_from_type(path, type);
  }

  template <typename Path, typename T>
  auto operator()(Path, omni::type_t<std::vector<T>>) const
    -> omni::ryml::map_range_t<Keep> {
    return omni::ryml::map_range.tdm_keep_nulls(
      omni::fn::default_value<Keep>{});
  }
};

TEST(keep_nulls, preserves_empty_optional_array_positions) {
  const std::vector<omni::compat::optional<int>> source{
    omni::compat::nullopt, omni::compat::make_optional(7),
    omni::compat::nullopt};
  const auto writer = omni::ryml::serialize.tdm_select_mapping(
    omni::fn::default_value<range_null_policy<>>{});

  EXPECT_TRUE(serialization_test::matches_json("[null, 7, null]",
    writer(source)));
}

struct measurement {
  omni::compat::optional<int> value;
  std::vector<omni::compat::optional<int>> samples;
};

// A named updater lets C++11 spell the strategy's return type below.
struct null_override {
  bool keep;

  constexpr bool operator()(omni::compat::monostate) const { return keep; }
};

struct select_null_policy {
  bool record;
  bool range;

  template <typename Path, typename T>
  auto operator()(Path path, omni::type_t<T> type) const
    -> decltype(omni::ryml::reflected_scope::fn::mapping_from_type(
      path, type)) {
    return omni::ryml::reflected_scope::fn::mapping_from_type(path, type);
  }

  template <typename Path>
  auto operator()(Path, omni::type_t<measurement>) const
    -> omni::ryml::reflected_scope::map_record_t<
      decltype(omni::mapping::record_strategy.tdm_keep_nulls(
        null_override{}))> {
    return omni::ryml::reflected_scope::map_record.tdm_strategy(
      [this](const omni::mapping::record_strategy_t<> &strategy) {
        return strategy.tdm_keep_nulls(null_override{this->record});
      });
  }

  template <typename Path, typename T>
  auto operator()(Path, omni::type_t<std::vector<T>>) const
    -> omni::ryml::map_range_t<bool> {
    return omni::ryml::map_range.tdm_keep_nulls(
      null_override{this->range});
  }
};

// Shared omission policy and local overrides determine emitted null values.
struct null_policy_case {
  std::string name;
  bool keep_fields;
  bool keep_elements;
  bool record_override;
  bool range_override;
  std::string expected;
};

class null_policy: public ::testing::TestWithParam<null_policy_case> {};

TEST_P(null_policy, serializes_record) {
  const auto &parameter = GetParam();
  const measurement source{omni::compat::nullopt,
    {omni::compat::make_optional(1), omni::compat::nullopt,
      omni::compat::make_optional(2)}};
  const auto writer = omni::ryml::serialize
    .tdm_config([&parameter](omni::ryml::serialize_config_t options) {
      options.keep_null_fields = parameter.keep_fields;
      options.keep_null_elements = parameter.keep_elements;
      return options;
    })
    .tdm_select_mapping(
      [&parameter](omni::ryml::reflected_scope::mapping_from_type_t) {
        return select_null_policy{parameter.record_override,
          parameter.range_override};
      });

  EXPECT_TRUE(serialization_test::matches_json(parameter.expected,
    writer(source)));
}

INSTANTIATE_TEST_SUITE_P(omission_and_overrides, null_policy,
  ::testing::Values(
    null_policy_case{"omit_both", true, true, false, false,
      R"({"samples":[1,2]})"},
    null_policy_case{"keep_both", false, false, true, true,
      R"({"value":null,"samples":[1,null,2]})"},
    null_policy_case{"keep_record", false, true, true, false,
      R"({"value":null,"samples":[1,2]})"},
    null_policy_case{"keep_range", true, false, false, true,
      R"({"samples":[1,null,2]})"}),
  [](const ::testing::TestParamInfo<null_policy_case> &parameter) {
    return parameter.param.name;
  });

TEST(keep_nulls, inherits_shared_policy) {
  const measurement source{omni::compat::nullopt,
    {omni::compat::make_optional(1), omni::compat::nullopt}};
  const auto writer = omni::ryml::serialize.tdm_config(
    [](omni::ryml::serialize_config_t options) {
      options.keep_null_fields = false;
      options.keep_null_elements = true;
      return options;
    });

  EXPECT_TRUE(serialization_test::matches_json(R"({"samples":[1,null]})",
    writer(source)));
}

TEST(keep_nulls, constant_false_overrides_shared_policy) {
  const std::vector<omni::compat::optional<int>> source{
    omni::compat::nullopt, omni::compat::make_optional(7)};
  const auto writer = omni::ryml::serialize
    .tdm_config([](omni::ryml::serialize_config_t options) {
      options.keep_null_elements = true;
      return options;
    })
    .tdm_select_mapping(
      omni::fn::default_value<range_null_policy<std::false_type>>{});

  EXPECT_TRUE(serialization_test::matches_json("[7]", writer(source)));
}

TEST(keep_nulls, preserves_null_root) {
  const omni::compat::optional<int> source;
  const auto writer = omni::ryml::serialize.tdm_config(
    [](omni::ryml::serialize_config_t options) {
      options.keep_null_fields = false;
      options.keep_null_elements = false;
      return options;
    });

  EXPECT_TRUE(serialization_test::matches_json("null", writer(source)));
}

TEST(keep_nulls, omits_empty_associative_values) {
  const std::map<std::string, omni::compat::optional<int>> source{
    {"empty", omni::compat::nullopt},
    {"value", omni::compat::make_optional(7)}};
  const auto writer = omni::ryml::serialize.tdm_config(
    [](omni::ryml::serialize_config_t options) {
      options.keep_null_fields = false;
      return options;
    });

  EXPECT_TRUE(serialization_test::matches_json(R"({"value":7})",
    writer(source)));
}

TEST(keep_nulls, maps_unsized_range) {
  const std::forward_list<omni::compat::optional<int>> source{
    omni::compat::make_optional(1), omni::compat::nullopt,
    omni::compat::make_optional(2)};
  const auto writer = omni::ryml::serialize.tdm_config(
    [](omni::ryml::serialize_config_t options) {
      options.keep_null_elements = true;
      return options;
    });

  EXPECT_TRUE(serialization_test::matches_json("[1,null,2]", writer(source)));
}

TEST(keep_nulls, preserves_nested_empty_optional_position) {
  const std::vector<omni::compat::optional<omni::compat::optional<int>>> source{
    omni::compat::make_optional(omni::compat::optional<int>{})};
  const auto writer = omni::ryml::serialize.tdm_select_mapping(
    omni::fn::default_value<range_null_policy<>>{});

  EXPECT_TRUE(serialization_test::matches_json("[null]", writer(source)));
}

struct result_item {
  omni::compat::optional<int> value;
};

TEST(keep_nulls, preserves_positions_without_forcing_record_fields) {
  const std::vector<omni::compat::optional<result_item>> source{
    omni::compat::nullopt,
    omni::compat::make_optional(result_item{omni::compat::nullopt})};
  const auto writer = omni::ryml::serialize
    .tdm_config([](omni::ryml::serialize_config_t options) {
      options.keep_null_fields = false;
      return options;
    })
    .tdm_select_mapping(
      omni::fn::default_value<range_null_policy<>>{});

  EXPECT_TRUE(serialization_test::matches_json("[null,{}]", writer(source)));
}

TEST(keep_nulls, tree_builder_keeps_policy_when_changing_selector) {
  const std::vector<omni::compat::optional<int>> source{
    omni::compat::nullopt, omni::compat::make_optional(7)};
  const auto build = omni::ryml::as_owning_tree
    .tdm_config([](omni::ryml::serialize_config_t options) {
      options.keep_null_elements = true;
      return options;
    })
    .tdm_select_mapping(omni::fn::identity{});
  const auto tree = build(source);

  EXPECT_TRUE(serialization_test::matches_json("[null,7]",
    ::ryml::emitrs_json<std::string>(tree)));
}

TEST(keep_nulls, format_update_keeps_null_policy) {
  const std::vector<omni::compat::optional<int>> source{
    omni::compat::nullopt, omni::compat::make_optional(7)};
  const auto writer = omni::ryml::serialize
    .tdm_config([](omni::ryml::serialize_config_t options) {
      options.keep_null_elements = true;
      return options;
    })
    .tdm_format([](::ryml::EmitType_e) { return ::ryml::EMIT_YAML; });

  EXPECT_TRUE(serialization_test::matches_json("[null,7]", writer(source)));
}

} // namespace
