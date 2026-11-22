/** Shared strategies through public JSON/YAML mapping entry points. */
#include "json_match.hpp"

#include <array>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace strategy_examples {

using omni::compat::string_view;

struct interval {
  int begin;
  int end;
  std::string label = "private";
  static int revision;
};

int interval::revision = 4;

struct schedule { std::vector<interval> intervals; };

struct nonnegative {
  auto operator()(int value) const
    -> omni::compat::expected<int, omni::ryml::issue> {
    if (value < 0)
      return omni::compat::unexpected<omni::ryml::issue>{
        omni::ryml::core::make_issue(omni::json::issue_code::out_of_range,
          omni::json::value_kind::integer, ::ryml::ConstNodeRef{})};
    return value;
  }
};

struct at_most_one_hundred {
  auto operator()(int value) const
    -> omni::compat::expected<int, omni::ryml::issue> {
    if (100 < value)
      return omni::compat::unexpected<omni::ryml::issue>{
        omni::ryml::core::make_issue(omni::json::issue_code::out_of_range,
          omni::json::value_kind::integer, ::ryml::ConstNodeRef{})};
    return value;
  }
};

struct review_label {
  auto operator()(std::string value) const
    -> omni::ryml::with_diagnostics_t<std::string> {
    omni::ryml::diagnostics report{};
    if (value == "review")
      report.issues.push_back(omni::ryml::core::make_issue(
        omni::json::issue_code::out_of_range, omni::json::value_kind::string,
        ::ryml::ConstNodeRef{}, /*warning=*/true));
    return {std::move(value), std::move(report)};
  }
};

struct increasing_interval {
  auto operator()(interval value) const
    -> omni::compat::expected<interval, omni::ryml::issue> {
    if (value.end < value.begin)
      return omni::compat::unexpected<omni::ryml::issue>{
        omni::ryml::core::make_issue(omni::json::issue_code::out_of_range,
          omni::json::value_kind::mapping, ::ryml::ConstNodeRef{})};
    return value;
  }
};

struct interval_aliases {
  auto operator()(const std::tuple<> &) const
    -> decltype(omni::mapping::aliases_dict(
      std::make_tuple("begin", "start"), std::make_tuple("end", "finish"))) {
    return omni::mapping::aliases_dict(
      std::make_tuple("begin", "start"), std::make_tuple("end", "finish"));
  }
};

struct interval_validators {
  auto operator()(const std::tuple<> &) const
    -> decltype(std::make_tuple(
      std::make_tuple("begin", nonnegative{}, at_most_one_hundred{}),
      std::make_tuple("end", nonnegative{}),
      std::make_tuple("label", review_label{}))) {
    return std::make_tuple(
      std::make_tuple("begin", nonnegative{}, at_most_one_hundred{}),
      std::make_tuple("end", nonnegative{}),
      std::make_tuple("label", review_label{}));
  }
};

const auto interval_strategy = omni::mapping::record_strategy
  .tdm_aliases_by_name(interval_aliases{})
  .tdm_validate_fields(interval_validators{})
  .tdm_finalize(omni::fn::default_value<increasing_interval>{});

struct include_statics {
  template <typename Strategy>
  constexpr auto operator()(const Strategy &strategy) const
    -> decltype(strategy.tdm_filter_fields(
      omni::fn::default_value<
        omni::compat::remove_cvref_t<decltype(omni::fn::always_true)>>{})) {
    return strategy.tdm_filter_fields(
      omni::fn::default_value<
        omni::compat::remove_cvref_t<decltype(omni::fn::always_true)>>{});
  }
};

struct without_label {
  template <typename _F>
  constexpr auto operator()(omni::data_member_meta_t<_F>) const
    -> omni::compat::bool_constant<
      'l' != omni::data_member_meta_t<_F>{}.name()[0]> { return {}; }
};

struct select_static_writer {
  template <typename State>
  auto operator()(State, omni::type_t<interval>) const
    -> decltype(omni::ryml::reflected_scope::from_record
      .tdm_strategy(include_statics{})) {
    return omni::ryml::reflected_scope::from_record
      .tdm_strategy(include_statics{});
  }

  template <typename State, typename T>
  auto operator()(State state, omni::type_t<T> type) const
    -> decltype(omni::ryml::reflected_scope::fn::mapping_from_type(
      state, type)) {
    return omni::ryml::reflected_scope::fn::mapping_from_type(state, type);
  }
};

enum class status { idle, busy };

struct status_aliases {
  auto operator()(const std::tuple<> &) const
    -> decltype(omni::mapping::aliases_dict(
      std::make_tuple("busy", "working"))) {
    return omni::mapping::aliases_dict(std::make_tuple("busy", "working"));
  }
};

const auto status_strategy = omni::mapping::enum_strategy
  .tdm_aliases_by_name(status_aliases{});

struct existing {
  int number;
  explicit existing(int value): number(value) {}
};

struct positive_existing {
  auto operator()(existing value) const
    -> omni::compat::expected<existing, omni::ryml::issue> {
    if (value.number < 0)
      return omni::compat::unexpected<omni::ryml::issue>{
        omni::ryml::core::make_issue(omni::json::issue_code::out_of_range,
          omni::json::value_kind::mapping, ::ryml::ConstNodeRef{})};
    return value;
  }
};

const auto existing_strategy = omni::mapping::record_strategy
  .tdm_finalize(omni::fn::default_value<positive_existing>{});

} // namespace strategy_examples

namespace omni {
namespace ryml {
namespace cp {

template <>
struct to_type_t<strategy_examples::interval> {
  OMNI_CONSTRAINED_TEMPLATE((typename State), (is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node,
    omni::type_t<strategy_examples::interval> type) const
    -> with_diagnostics_t<strategy_examples::interval> {
    return reflected_scope::default_to_record(type,
      strategy_examples::interval_strategy)(std::move(state), node, type);
  }
};

template <>
struct to_type_t<strategy_examples::status> {
  OMNI_CONSTRAINED_TEMPLATE((typename State), (is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node,
    omni::type_t<strategy_examples::status> type) const
    -> with_diagnostics_t<strategy_examples::status> {
    return reflected_scope::default_to_enum(type,
      strategy_examples::status_strategy)(std::move(state), node, type);
  }
};

template <>
struct to_type_t<strategy_examples::existing> {
  OMNI_CONSTRAINED_TEMPLATE((typename State), (is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node,
    strategy_examples::existing *into) const
    -> with_diagnostics_t<compat::monostate> {
    return reflected_scope::default_to_record(
      omni::type_t<strategy_examples::existing>{},
      strategy_examples::existing_strategy)(std::move(state), node, into);
  }
};

} // namespace cp
} // namespace ryml
} // namespace omni

TEST(mapping_strategy, aliases_and_field_validation_keep_canonical_output) {
  const auto result = serialization_test::deserialize(
    omni::type_t<strategy_examples::interval>{},
    R"({"start":1,"finish":3,"label":"ok"})");
  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_EQ(1, result.value->begin);
    EXPECT_EQ(3, result.value->end);
    EXPECT_TRUE(serialization_test::matches_json(
      R"({"begin":1,"end":3,"label":"ok"})",
      omni::ryml::as_json(*result.value)));
  }
}

struct invalid_interval_case {
  std::string document;
  std::string field;
};

class field_strategy_validation:
    public testing::TestWithParam<invalid_interval_case> {};

TEST_P(field_strategy_validation, reports_the_rejected_field) {
  const auto &input = GetParam();
  const auto result = serialization_test::deserialize(
    omni::type_t<strategy_examples::interval>{}, input.document);
  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (result.diagnostics.issues.size() == 1) {
    EXPECT_EQ(omni::json::issue_code::out_of_range,
      result.diagnostics.issues.front().reason);
    EXPECT_EQ(input.field, omni::ryml::detail::render_path(
      result.diagnostics.issues.front().path));
  }
}

INSTANTIATE_TEST_SUITE_P(bounds, field_strategy_validation, testing::Values(
  invalid_interval_case{R"({"start":-1,"finish":3,"label":"ok"})", "/start"},
  invalid_interval_case{R"({"start":101,"finish":103,"label":"ok"})", "/start"},
  invalid_interval_case{R"({"start":1,"finish":-3,"label":"ok"})", "/finish"}));

TEST(mapping_strategy, nested_finalization_stops_before_later_items) {
  const auto result = serialization_test::deserialize(
    omni::type_t<strategy_examples::schedule>{},
    R"({"intervals":[{"start":5,"finish":3,"label":"ok"},false]})");
  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (result.diagnostics.issues.size() == 1) {
    EXPECT_EQ("/intervals/0", omni::ryml::detail::render_path(
      result.diagnostics.issues.front().path));
  }
}

TEST(mapping_strategy, filtered_fields_keep_their_defaults) {
  const auto strategy = strategy_examples::interval_strategy
    .tdm_filter_fields(
      omni::fn::default_value<strategy_examples::without_label>{});
  const auto mapping = omni::ryml::reflected_scope::default_to_record(
    omni::type_t<strategy_examples::interval>{}, strategy);
  const auto tree = omni::ryml::core::parse(R"({"start":1,"finish":3})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;
  const auto result = serialization_test::map_using(omni::ryml::config_t<>{},
    mapping, omni::type_t<strategy_examples::interval>{}, tree->crootref());
  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_EQ("private", result.value->label);
  }
#if defined(__cpp_designated_initializers) \
  && 201707L <= __cpp_designated_initializers
  const decltype(strategy) designated{
    .filter_fields = {},
    .aliases_by_name = strategy.aliases_by_name,
    .validate_fields = strategy.validate_fields,
    .finalize = strategy.finalize,
  };
  const auto direct = serialization_test::map_using(omni::ryml::config_t<>{},
    omni::ryml::reflected_scope::default_to_record(
      omni::type_t<strategy_examples::interval>{}, designated),
    omni::type_t<strategy_examples::interval>{}, tree->crootref());
  EXPECT_TRUE(direct.value);
  if (direct.value) {
    EXPECT_EQ(1, direct.value->begin);
    EXPECT_EQ(3, direct.value->end);
    EXPECT_EQ("private", direct.value->label);
  }
#endif
}

TEST(mapping_strategy, static_fields_are_opt_in_for_serialization_only) {
  const auto serialize = omni::ryml::serialize
    .tdm_map_node(omni::fn::default_value<
      strategy_examples::select_static_writer>{});
  strategy_examples::interval value{};
  value.begin = 1;
  value.end = 3;
  value.label = "ok";
  EXPECT_TRUE(serialization_test::matches_json(
    R"({"begin":1,"end":3,"label":"ok","revision":4})", serialize(value)));

  const auto strategy = strategy_examples::include_statics{}(
    omni::mapping::record_strategy);
  const auto mapping = omni::ryml::reflected_scope::default_to_record(
    omni::type_t<strategy_examples::interval>{}, strategy);
  const auto tree = omni::ryml::core::parse(
    R"({"begin":1,"end":3,"label":"ok"})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;
  const auto result = serialization_test::map_using(omni::ryml::config_t<>{},
    mapping, omni::type_t<strategy_examples::interval>{}, tree->crootref());
  EXPECT_TRUE(result.value);
  EXPECT_EQ(4, strategy_examples::interval::revision);
}

TEST(mapping_strategy, enum_aliases_preserve_serialized_names) {
  const auto result = serialization_test::deserialize(
    omni::type_t<strategy_examples::status>{}, R"("working")");
  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_EQ(strategy_examples::status::busy, *result.value);
    EXPECT_EQ(R"("busy")", omni::ryml::as_json(*result.value));
  }
}

TEST(mapping_strategy, finalization_accepts_an_existing_nondefault_root) {
  strategy_examples::existing value(7);
  const auto result = serialization_test::deserialize(
    &value, R"({"number":11})");
  EXPECT_TRUE(result.value);
  EXPECT_EQ(11, value.number);
  const auto rejected = serialization_test::deserialize(
    &value, R"({"number":-1})");
  EXPECT_FALSE(rejected.value);
  EXPECT_EQ(1U, rejected.diagnostics.issues.size());
}

TEST(mapping_strategy, validation_warnings_obey_the_active_warning_limit) {
  const auto accepted = serialization_test::deserialize(
    omni::type_t<strategy_examples::interval>{},
    R"({"start":1,"finish":3,"label":"review"})");
  EXPECT_TRUE(accepted.value);
  EXPECT_EQ(1U, accepted.diagnostics.issues.size());
  if (accepted.diagnostics.issues.size() == 1) {
    EXPECT_TRUE(accepted.diagnostics.issues.front().warning);
    EXPECT_EQ("/label", omni::ryml::detail::render_path(
      accepted.diagnostics.issues.front().path));
  }
  const auto deserialize = serialization_test::deserialize
    .tdm_config([](serialization_test::config config) {
      config.error_policy.warning_limit = 0;
      return config;
    });
  const auto rejected = deserialize(
    omni::type_t<strategy_examples::interval>{},
    R"({"start":1,"finish":3,"label":"review"})");
  EXPECT_FALSE(rejected.value);
  EXPECT_EQ(2U, rejected.diagnostics.issues.size());
  if (rejected.diagnostics.issues.size() == 2) {
    EXPECT_EQ(omni::json::issue_code::warning_limit_exceeded,
      rejected.diagnostics.issues.back().reason);
  }
}

TEST(mapping_strategy, checks_alias_names_and_collisions) {
#if 202002L <= OMNI_CPLUSPLUS
  constexpr std::array<omni::compat::string_view, 2> known{{"begin", "end"}};
  constexpr auto aliases = omni::mapping::aliases_dict(
    std::tuple{"begin", "start"});
  static_assert(omni::mapping::valid_aliases(known, aliases),
    "Known, unambiguous aliases can be checked at compile time");
#endif
  const std::array<omni::compat::string_view, 2> names{{"begin", "end"}};
  EXPECT_TRUE(omni::mapping::valid_aliases(names,
    omni::mapping::aliases_dict(std::make_tuple("begin", "start"))));
  EXPECT_FALSE(omni::mapping::resolve_alias(
    omni::mapping::aliases_dict(std::make_tuple("begin")), "start"));
  EXPECT_FALSE(omni::mapping::valid_aliases(names,
    omni::mapping::aliases_dict(std::make_tuple("missing", "start"))));
  EXPECT_FALSE(omni::mapping::valid_aliases(names,
    omni::mapping::aliases_dict(std::make_tuple("begin", "end"))));
  EXPECT_FALSE(omni::mapping::valid_aliases(names,
    omni::mapping::aliases_dict(std::make_tuple("begin", "same"),
      std::make_tuple("end", "same"))));
}
