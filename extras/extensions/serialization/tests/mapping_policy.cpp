/** Shared policies through public JSON/YAML mapping entry points. */
#include "json_match.hpp"

#include <array>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace policy_examples {

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

const auto interval_policy = omni::mapping::record_policy
  .tdm_aliases_by_name(interval_aliases{})
  .tdm_validate_fields(interval_validators{})
  .tdm_finalize(omni::fn::default_value<increasing_interval>{});

struct include_statics {
  template <typename Policy>
  constexpr auto operator()(const Policy &policy) const
    -> decltype(policy.tdm_filter_fields(
      omni::fn::default_value<
        omni::compat::remove_cvref_t<decltype(omni::fn::always_true)>>{})) {
    return policy.tdm_filter_fields(
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
  template <typename Path>
  auto operator()(Path, omni::type_t<interval>) const
    -> decltype(omni::ryml::reflected_scope::map_record
      .tdm_policy(include_statics{})) {
    return omni::ryml::reflected_scope::map_record
      .tdm_policy(include_statics{});
  }

  template <typename Path, typename T>
  auto operator()(Path path, omni::type_t<T> type) const
    -> decltype(omni::ryml::reflected_scope::fn::mapping_from_type(
      path, type)) {
    return omni::ryml::reflected_scope::fn::mapping_from_type(path, type);
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

const auto status_policy = omni::mapping::enum_policy
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

const auto existing_policy = omni::mapping::record_policy
  .tdm_finalize(omni::fn::default_value<positive_existing>{});

struct copy_only_aliases {
  copy_only_aliases() = default;
  copy_only_aliases(const copy_only_aliases &) = default;
  copy_only_aliases(copy_only_aliases &&) = delete;

  omni::compat::optional<string_view> operator()(string_view name) const {
    return name == "legacy" ? omni::compat::make_optional(string_view{"name"})
                            : omni::compat::nullopt;
  }
};

struct owning_check {
  std::unique_ptr<int> limit;

  auto operator()(int value) const -> omni::compat::expected<int, string_view> {
    if (value < *this->limit)
      return omni::compat::unexpected<string_view>{"below limit"};
    return value;
  }
};

struct update_aliases {
  template <typename Policy>
  auto operator()(Policy &&policy) const
    -> decltype(std::forward<Policy>(policy).tdm_aliases_by_name(
      omni::fn::identity{})) {
    return std::forward<Policy>(policy).tdm_aliases_by_name(
      omni::fn::identity{});
  }
};

} // namespace policy_examples

namespace optional_policy_examples {

struct settings { omni::compat::optional<int> value; };

struct require_value {
  auto operator()(omni::compat::optional<int> value) const
    -> omni::compat::expected<omni::compat::optional<int>, omni::ryml::issue> {
    if (!value)
      return omni::compat::unexpected<omni::ryml::issue>{
        omni::ryml::core::make_issue(omni::json::issue_code::out_of_range,
          omni::json::value_kind::integer, ::ryml::ConstNodeRef{})};
    return value;
  }
};

struct field_checks {
  auto operator()(const std::tuple<> &) const
    -> decltype(std::make_tuple(std::make_tuple("value", require_value{}))) {
    return std::make_tuple(std::make_tuple("value", require_value{}));
  }
};

} // namespace optional_policy_examples

namespace numeric_policy_examples {

struct settings {
  int age;
  double ratio;
  long total;
};

struct field_checks {
  auto operator()(const std::tuple<> &) const
    -> decltype(std::make_tuple(
      std::make_tuple("age", policy_examples::nonnegative{}))) {
    return std::make_tuple(
      std::make_tuple("age", policy_examples::nonnegative{}));
  }
};

} // namespace numeric_policy_examples

namespace filtered_root_examples {

struct token {
  int value;
  explicit token(int initial): value(initial) {}
};

struct settings {
  token retained;
  int parsed;
};

struct parsed_only {
  template <typename _F>
  constexpr auto operator()(omni::data_member_meta_t<_F>) const
    -> omni::compat::bool_constant<
      'p' == omni::data_member_meta_t<_F>{}.name()[0]> { return {}; }
};

} // namespace filtered_root_examples

namespace generic_policy_examples {

struct settings {
  int age;
  std::string name;
};

struct nonnegative {
  template <typename T>
  auto operator()(T value) const -> omni::compat::expected<T, omni::ryml::issue> {
    if (value < 0)
      return omni::compat::unexpected<omni::ryml::issue>{
        omni::ryml::core::make_issue(omni::json::issue_code::out_of_range,
          omni::json::value_kind::integer, ::ryml::ConstNodeRef{})};
    return value;
  }
};

struct nonempty {
  template <typename T>
  auto operator()(T value) const -> omni::compat::expected<T, omni::ryml::issue> {
    if (value.empty())
      return omni::compat::unexpected<omni::ryml::issue>{
        omni::ryml::core::make_issue(omni::json::issue_code::out_of_range,
          omni::json::value_kind::string, ::ryml::ConstNodeRef{})};
    return value;
  }
};

const auto age_key = OMNI_META_STRING("age");
const auto name_key = OMNI_META_STRING("name");

struct field_checks {
  auto operator()(const std::tuple<> &) const
    -> decltype(std::make_tuple(std::make_tuple(age_key, nonnegative{}),
      std::make_tuple(name_key, nonempty{}))) {
    return std::make_tuple(std::make_tuple(age_key, nonnegative{}),
      std::make_tuple(name_key, nonempty{}));
  }
};

} // namespace generic_policy_examples

namespace omni {
namespace ryml {
namespace cp {

template <>
struct to_type_t<numeric_policy_examples::settings> {
  OMNI_CONSTRAINED_TEMPLATE((typename State), (is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node,
    omni::type_t<numeric_policy_examples::settings> type) const
    -> with_diagnostics_t<numeric_policy_examples::settings> {
    const auto policy = mapping::record_policy
      .tdm_validate_fields(numeric_policy_examples::field_checks{});
    return reflected_scope::map_object_node.tdm_policy(
      [&policy](const mapping::record_policy_t<> &) {
        return policy;
      })(
      std::move(state), node, type);
  }
};

template <>
struct to_type_t<filtered_root_examples::settings> {
  // Delegate because the mapper is final: `.tdm_` updates return its own type
  // and cannot preserve members added by derived registrations.
  static constexpr auto record_mapping()
    -> decltype(reflected_scope::map_object_node_t<mapping::record_policy_t<
      filtered_root_examples::parsed_only>>{}) {
    return reflected_scope::map_object_node_t<mapping::record_policy_t<
      filtered_root_examples::parsed_only>>{};
  }

  template <typename... Args>
  auto operator()(Args &&...args) const
    -> decltype(record_mapping()(std::forward<Args>(args)...)) {
    constexpr auto mapping = record_mapping();
    return mapping(std::forward<Args>(args)...);
  }
};

template <>
struct to_type_t<generic_policy_examples::settings> {
  OMNI_CONSTRAINED_TEMPLATE((typename State), (is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node,
    omni::type_t<generic_policy_examples::settings> type) const
    -> with_diagnostics_t<generic_policy_examples::settings> {
    const auto policy = mapping::record_policy
      .tdm_validate_fields(generic_policy_examples::field_checks{});
    return reflected_scope::map_object_node.tdm_policy(
      [&policy](const mapping::record_policy_t<> &) {
        return policy;
      })(
      std::move(state), node, type);
  }
};

template <>
struct to_type_t<optional_policy_examples::settings> {
  OMNI_CONSTRAINED_TEMPLATE((typename State), (is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node,
    omni::type_t<optional_policy_examples::settings> type) const
    -> with_diagnostics_t<optional_policy_examples::settings> {
    const auto policy = mapping::record_policy
      .tdm_validate_fields(optional_policy_examples::field_checks{});
    return reflected_scope::map_object_node.tdm_policy(
      [&policy](const mapping::record_policy_t<> &) {
        return policy;
      })(
      std::move(state), node, type);
  }
};

template <>
struct to_type_t<policy_examples::interval> {
  OMNI_CONSTRAINED_TEMPLATE((typename State), (is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node,
    omni::type_t<policy_examples::interval> type) const
    -> with_diagnostics_t<policy_examples::interval> {
    return reflected_scope::map_object_node.tdm_policy(
      [](const mapping::record_policy_t<> &) {
        return policy_examples::interval_policy;
      })(std::move(state), node, type);
  }
};

template <>
struct to_type_t<policy_examples::status> {
  OMNI_CONSTRAINED_TEMPLATE((typename State), (is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node,
    omni::type_t<policy_examples::status> type) const
    -> with_diagnostics_t<policy_examples::status> {
    return reflected_scope::map_node_to_enum.tdm_policy(
      [](const mapping::enum_policy_t<> &) {
        return policy_examples::status_policy;
      })(std::move(state), node, type);
  }
};

template <>
struct to_type_t<policy_examples::existing> {
  OMNI_CONSTRAINED_TEMPLATE((typename State), (is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node,
    policy_examples::existing *into) const
    -> with_diagnostics_t<compat::monostate> {
    return reflected_scope::map_object_node.tdm_policy(
      [](const mapping::record_policy_t<> &) {
        return policy_examples::existing_policy;
      })(std::move(state), node, into);
  }
};

} // namespace cp
} // namespace ryml
} // namespace omni

TEST(mapping_policy, aliases_and_field_validation_keep_canonical_output) {
  const auto result = serialization_test::deserialize(
    omni::type_t<policy_examples::interval>{},
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

class field_policy_validation:
    public testing::TestWithParam<invalid_interval_case> {};

TEST_P(field_policy_validation, reports_the_rejected_field) {
  const auto &input = GetParam();
  const auto result = serialization_test::deserialize(
    omni::type_t<policy_examples::interval>{}, input.document);
  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (result.diagnostics.issues.size() == 1) {
    EXPECT_EQ(omni::json::issue_code::out_of_range,
      result.diagnostics.issues.front().reason);
    EXPECT_EQ(input.field, omni::ryml::detail::render_path(
      result.diagnostics.issues.front().path));
  }
}

INSTANTIATE_TEST_SUITE_P(bounds, field_policy_validation, testing::Values(
  invalid_interval_case{R"({"start":-1,"finish":3,"label":"ok"})", "/start"},
  invalid_interval_case{R"({"start":101,"finish":103,"label":"ok"})", "/start"},
  invalid_interval_case{R"({"start":1,"finish":-3,"label":"ok"})", "/finish"}));

TEST(mapping_policy, nested_finalization_stops_before_later_items) {
  const auto result = serialization_test::deserialize(
    omni::type_t<policy_examples::schedule>{},
    R"({"intervals":[{"start":5,"finish":3,"label":"ok"},false]})");
  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (result.diagnostics.issues.size() == 1) {
    EXPECT_EQ("/intervals/0", omni::ryml::detail::render_path(
      result.diagnostics.issues.front().path));
  }
}

TEST(mapping_policy, filtered_fields_keep_their_defaults) {
  const auto policy = policy_examples::interval_policy
    .tdm_filter_fields(
      omni::fn::default_value<policy_examples::without_label>{});
  const auto mapping = omni::ryml::reflected_scope::map_object_node
    .tdm_policy([&policy](const omni::mapping::record_policy_t<> &) {
      return policy;
    });
  const auto tree = omni::ryml::core::parse(R"({"start":1,"finish":3})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;
  const auto result = serialization_test::map_using(omni::ryml::config_t<>{},
    mapping, omni::type_t<policy_examples::interval>{}, tree->crootref());
  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_EQ("private", result.value->label);
  }
#if defined(__cpp_designated_initializers) \
  && 201707L <= __cpp_designated_initializers
  const decltype(policy) designated{
    .filter_fields = {},
    .aliases_by_name = policy.aliases_by_name,
    .validate_fields = policy.validate_fields,
    .finalize = policy.finalize,
  };
  const auto direct = serialization_test::map_using(omni::ryml::config_t<>{},
    omni::ryml::reflected_scope::map_object_node.tdm_policy(
      [&designated](const omni::mapping::record_policy_t<> &) {
        return designated;
      }),
    omni::type_t<policy_examples::interval>{}, tree->crootref());
  EXPECT_TRUE(direct.value);
  if (direct.value) {
    EXPECT_EQ(1, direct.value->begin);
    EXPECT_EQ(3, direct.value->end);
    EXPECT_EQ("private", direct.value->label);
  }
#endif
}

TEST(mapping_policy, static_fields_are_opt_in_for_serialization_only) {
  const auto serialize = omni::ryml::serialize
    .tdm_select_mapping(omni::fn::default_value<
      policy_examples::select_static_writer>{});
  policy_examples::interval value{};
  value.begin = 1;
  value.end = 3;
  value.label = "ok";
  EXPECT_TRUE(serialization_test::matches_json(
    R"({"begin":1,"end":3,"label":"ok","revision":4})", serialize(value)));

  const auto policy = policy_examples::include_statics{}(
    omni::mapping::record_policy);
  const auto mapping = omni::ryml::reflected_scope::map_object_node
    .tdm_policy([&policy](const omni::mapping::record_policy_t<> &) {
      return policy;
    });
  const auto tree = omni::ryml::core::parse(
    R"({"begin":1,"end":3,"label":"ok"})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;
  const auto result = serialization_test::map_using(omni::ryml::config_t<>{},
    mapping, omni::type_t<policy_examples::interval>{}, tree->crootref());
  EXPECT_TRUE(result.value);
  EXPECT_EQ(4, policy_examples::interval::revision);
}

TEST(mapping_policy, enum_aliases_preserve_serialized_names) {
  const auto result = serialization_test::deserialize(
    omni::type_t<policy_examples::status>{}, R"("working")");
  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_EQ(policy_examples::status::busy, *result.value);
    EXPECT_EQ(R"("busy")", omni::ryml::as_json(*result.value));
  }
}

TEST(mapping_policy, finalization_accepts_an_existing_nondefault_root) {
  policy_examples::existing value(7);
  const auto result = serialization_test::deserialize(
    &value, R"({"number":11})");
  EXPECT_TRUE(result.value);
  EXPECT_EQ(11, value.number);
  const auto rejected = serialization_test::deserialize(
    &value, R"({"number":-1})");
  EXPECT_FALSE(rejected.value);
  EXPECT_EQ(1U, rejected.diagnostics.issues.size());
}

TEST(mapping_policy, validation_warnings_obey_the_active_warning_limit) {
  const auto accepted = serialization_test::deserialize(
    omni::type_t<policy_examples::interval>{},
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
    omni::type_t<policy_examples::interval>{},
    R"({"start":1,"finish":3,"label":"review"})");
  EXPECT_FALSE(rejected.value);
  EXPECT_EQ(2U, rejected.diagnostics.issues.size());
  if (rejected.diagnostics.issues.size() == 2) {
    EXPECT_EQ(omni::json::issue_code::warning_limit_exceeded,
      rejected.diagnostics.issues.back().reason);
  }
}

TEST(mapping_policy, checks_alias_names_and_collisions) {
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

struct optional_validation_case {
  std::string name;
  std::string document;
  bool accepted;
  omni::json::value_kind actual;
};

class optional_policy_validation:
    public testing::TestWithParam<optional_validation_case> {};

TEST_P(optional_policy_validation, validates_the_decoded_optional) {
  const auto &input = GetParam();
  const auto result = serialization_test::deserialize(
    omni::type_t<optional_policy_examples::settings>{}, input.document);
  EXPECT_EQ(input.accepted, result.value.has_value());
  if (input.accepted) {
    EXPECT_TRUE(result.diagnostics.issues.empty());
    if (result.value) {
      EXPECT_EQ(7, result.value->value);
    }
  } else {
    EXPECT_EQ(1U, result.diagnostics.issues.size());
    if (result.diagnostics.issues.size() == 1) {
      EXPECT_EQ(omni::json::issue_code::out_of_range,
        result.diagnostics.issues.front().reason);
      EXPECT_EQ("/value", omni::ryml::detail::render_path(
        result.diagnostics.issues.front().path));
      EXPECT_EQ(input.actual, result.diagnostics.issues.front().actual);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(presence, optional_policy_validation,
  testing::Values(
    optional_validation_case{"omitted", "{}", false,
      omni::json::value_kind::unknown},
    optional_validation_case{"null", R"({"value":null})", false,
      omni::json::value_kind::null},
    optional_validation_case{"present", R"({"value":7})", true,
      omni::json::value_kind::integer}),
  [](const testing::TestParamInfo<optional_validation_case> &input) {
    return input.param.name;
  });

TEST(mapping_policy, integer_validation_leaves_other_scalar_types_unchanged) {
  const auto result = serialization_test::deserialize(
    omni::type_t<numeric_policy_examples::settings>{},
    R"({"age":7,"ratio":1.25,"total":42})");
  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_EQ(7, result.value->age);
    EXPECT_DOUBLE_EQ(1.25, result.value->ratio);
    EXPECT_EQ(42, result.value->total);
  }
}

TEST(mapping_policy, filtered_nondefault_root_uses_its_pointer_mapping) {
  filtered_root_examples::settings value{filtered_root_examples::token{19}, 1};
  const auto result = serialization_test::deserialize(
    &value, R"({"parsed":7})");
  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  EXPECT_EQ(19, value.retained.value);
  EXPECT_EQ(7, value.parsed);
}

struct generic_validation_case {
  std::string name;
  std::string document;
  std::string rejected_field;
};

class typed_key_policy_validation:
    public testing::TestWithParam<generic_validation_case> {};

TEST_P(typed_key_policy_validation, binds_generic_checks_to_the_named_type) {
  const auto &input = GetParam();
  const auto result = serialization_test::deserialize(
    omni::type_t<generic_policy_examples::settings>{}, input.document);
  EXPECT_EQ(input.rejected_field.empty(), result.value.has_value());
  if (input.rejected_field.empty()) {
    EXPECT_TRUE(result.diagnostics.issues.empty());
    if (result.value) {
      EXPECT_EQ(7, result.value->age);
      EXPECT_EQ("Alice", result.value->name);
    }
  } else {
    EXPECT_EQ(1U, result.diagnostics.issues.size());
    if (result.diagnostics.issues.size() == 1) {
      EXPECT_EQ(input.rejected_field, omni::ryml::detail::render_path(
        result.diagnostics.issues.front().path));
    }
  }
}

INSTANTIATE_TEST_SUITE_P(fields, typed_key_policy_validation,
  testing::Values(
    generic_validation_case{"valid", R"({"age":7,"name":"Alice"})", ""},
    generic_validation_case{"negative", R"({"age":-1,"name":"Alice"})", "/age"},
    generic_validation_case{"empty", R"({"age":7,"name":""})", "/name"}),
  [](const testing::TestParamInfo<generic_validation_case> &input) {
    return input.param.name;
  });

TEST(mapping_policy, functional_updates_preserve_other_properties) {
  const omni::mapping::record_policy_t<
    decltype(omni::mapping::record_policy.filter_fields),
    policy_examples::copy_only_aliases> original{};
  const auto updated = original.tdm_filter_fields(omni::fn::identity{});
  EXPECT_EQ(omni::compat::string_view{"name"},
    omni::mapping::resolve_alias(updated.aliases_by_name, "legacy"));

  using owning_policy = omni::mapping::record_policy_t<
    decltype(omni::mapping::record_policy.filter_fields), std::tuple<>,
    std::tuple<>, policy_examples::owning_check>;
  static_assert(!omni::compat::is_invocable<policy_examples::update_aliases,
    const owning_policy &>::value,
    "A const update cannot copy a move-only property");
  static_assert(omni::compat::is_invocable<policy_examples::update_aliases,
    owning_policy &&>::value,
    "A consuming update can move that property");
  owning_policy owned{};
  owned.finalize.limit.reset(new int{4});
  const auto consumed = policy_examples::update_aliases{}(std::move(owned));
  EXPECT_EQ(7, consumed.finalize(7));
  const auto rejected = consumed.finalize(3);
  EXPECT_FALSE(rejected);
  if (!rejected) {
    EXPECT_EQ(omni::compat::string_view{"below limit"}, rejected.error());
  }
}

TEST(mapping_policy, node_check_updates_preserve_the_record_policy) {
  using omni::ryml::reflected_scope::map_object_node;
  const auto unchecked = map_object_node
    .tdm_with_node_check(omni::fn::default_value<std::false_type>{});
  const auto mapping = unchecked.tdm_policy(
    [](const omni::mapping::record_policy_t<> &) {
      return policy_examples::interval_policy;
    });
  static_assert(!decltype(mapping.with_node_check)::value,
    "Updating the policy retains the disabled node check");

  const auto checked = mapping.tdm_with_node_check(
    omni::fn::default_value<std::true_type>{});

  const auto tree = omni::ryml::core::parse(
    R"({"start": 2, "finish": 7, "label": "meeting"})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto result = serialization_test::map_using(omni::ryml::config_t<>{},
    checked, omni::type_t<policy_examples::interval>{}, tree->crootref());
  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_EQ(2, result.value->begin);
    EXPECT_EQ(7, result.value->end);
    EXPECT_EQ("meeting", result.value->label);
  }

  const auto sequence = omni::ryml::core::parse("[]");
  EXPECT_TRUE(sequence);
  if (!sequence)
    return;

  const auto mismatch = serialization_test::map_using(
    omni::ryml::config_t<>{}, checked,
    omni::type_t<policy_examples::interval>{}, sequence->crootref());
  EXPECT_FALSE(mismatch.value);
  EXPECT_EQ(1U, mismatch.diagnostics.issues.size());
  if (!mismatch.diagnostics.issues.empty()) {
    EXPECT_EQ(omni::json::value_kind::mapping,
      mismatch.diagnostics.issues.front().expected);
  }
}
