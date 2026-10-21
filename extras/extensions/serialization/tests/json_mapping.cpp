// Intended coverage (pseudo form):
//   engine:       ryml; rapidjson later
//   struct:       C++ structs, instantiated at compile time
//   options:      combinations of 1..n policies, e.g. duplicates + error policy
//   json:         input documents, preserving repeated field names
//
//   cases = engine x struct x options x json
//   expected = specified_result(struct, options, json)
//
// Engines share the same expectations. A document's expected result depends on
// both the struct and policies; it is not a separate Cartesian axis.

/*
S = struct { int value; }
duplicates: duplicate_policy
D(duplicates, JSON) = (S or none, issues, warnings)
none = no value; [] = no issues
Edup = duplicate-field error at /value
Etype = invalid-integer error at /value
Wdup = duplicate-field warning at /value
error_limit = 0; warning_limit = none (unlimited); recovery = nullopt.
Omitted policy fields = defaults.

D(reject, {"value": 7})                         = ({7}, [], 0)
D(reject, {"value": 7, "value": 42})            = (none, [Edup], 0)
D(allow,  {"value": 7, "value": 42})            = ({7}, [], 0)
D(allow,  {"value": 7, "value": "bad"})         = ({7}, [], 0)
D(allow,  {"value": "bad", "value": 42})        = (none, [Etype], 0)

D(last, {"value": 7, "value": 42})    = ({42}, [], 0)
D(last, {"value": "bad", "value": 42}) = ({42}, [], 0)
D(last, {"value": 7, "value": "bad"}) = (none, [Etype], 0)

D(warn, {"value": 7, "value": 42}) = (none, [Edup], 0)
D(allow | warn, {"value": 7, "value": 42}) = ({7}, [Wdup], 1)

D(last | warn, {"value": 7, "value": 42}) = ({42}, [Wdup], 1)
D(allow | last, JSON) = D(last, JSON)
D(reject | warn, JSON) = D(reject, JSON)

For every engine:
  engine(S, error_policy{duplicates: duplicates}, JSON) = D(duplicates, JSON)

unknown_fields: unknown_policy
U(unknown_fields, JSON) = (S or none, issues, warnings)
Eunknown = unknown-field error at /other
Wunknown = unknown-field warning at /other

U(reject, {"value": 7, "other": 42}) = (none, [Eunknown], 0)
U(allow,  {"value": 7, "other": 42}) = ({7}, [], 0)
U(warn,   {"value": 7, "other": 42}) = (none, [Eunknown], 0)
U(allow | warn, {"value": 7, "other": 42}) = ({7}, [Wunknown], 1)
U(reject | warn, JSON) = U(reject, JSON)

For every engine:
  engine(S, error_policy{unknown_fields: unknown_fields}, JSON)
    = U(unknown_fields, JSON)

warnings(error_policy{duplicates: allow | warn, unknown_fields: allow | warn},
  {"value": 7, "value": 42, "other": 5}) = 2
*/

#include <omnirefl/serialization/ryml.hpp>

#include <gtest/gtest.h>

#include <string>

namespace {

struct record {
  int value = 17;
};

omni::ryml::with_diagnostics<record, std::string>
  deserialize_ryml(omni::ryml::settings options, std::string input) {
  return omni::ryml::core::deserialize(options,
    omni::type_t<record>{},
    std::move(input))
    .transform_error(omni::ryml::render_diagnostics);
}

struct engine {
  // Backend label used in the parameterized test name.
  std::string name;

  // Invoke the backend and render its retained diagnostics.
  omni::ryml::with_diagnostics<record, std::string> (
    *deserialize)(omni::ryml::settings, std::string);
};

class json_mapping: public testing::TestWithParam<engine> {};

TEST_P(json_mapping, rejects_duplicates_by_default) {
  omni::ryml::settings options;
  const auto result =
    GetParam().deserialize(options, R"({"value": 7, "value": 42})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/value: duplicate field", result.diagnostics);
}

TEST_P(json_mapping, warn_alone_does_not_allow_duplicates) {
  omni::ryml::settings options;
  options.error_policy.duplicates = omni::json::duplicate_policy::warn;
  const auto result =
    GetParam().deserialize(options, R"({"value": 7, "value": 42})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/value: duplicate field", result.diagnostics);
}

TEST_P(json_mapping, first_ignores_invalid_discarded_value) {
  omni::ryml::settings options;
  options.error_policy.duplicates = omni::json::duplicate_policy::allow;
  const auto result =
    GetParam().deserialize(options, R"({"value": 7, "value": "bad"})");

  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_EQ(7, result.value->value);
  }
  EXPECT_EQ("", result.diagnostics);
}

TEST_P(json_mapping, last_ignores_invalid_discarded_value) {
  omni::ryml::settings options;
  options.error_policy.duplicates = omni::json::duplicate_policy::last;
  const auto result =
    GetParam().deserialize(options, R"({"value": "bad", "value": 42})");

  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_EQ(42, result.value->value);
  }
  EXPECT_EQ("", result.diagnostics);
}

TEST_P(json_mapping, first_validates_selected_value) {
  omni::ryml::settings options;
  options.error_policy.duplicates = omni::json::duplicate_policy::allow;
  const auto result =
    GetParam().deserialize(options, R"({"value": "bad", "value": 42})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/value: \"bad\" is not an integer", result.diagnostics);
}

TEST_P(json_mapping, last_validates_selected_value) {
  omni::ryml::settings options;
  options.error_policy.duplicates = omni::json::duplicate_policy::last;
  const auto result =
    GetParam().deserialize(options, R"({"value": 7, "value": "bad"})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/value: \"bad\" is not an integer", result.diagnostics);
}

TEST_P(json_mapping, returns_valid_value_with_duplicate_warning) {
  omni::ryml::settings options;
  options.error_policy.duplicates =
    omni::json::duplicate_policy::allow | omni::json::duplicate_policy::warn;
  const auto result =
    GetParam().deserialize(options, R"({"value": 7, "value": 42})");

  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_EQ(7, result.value->value);
  }
  EXPECT_EQ("/value: warning: duplicate field", result.diagnostics);
}

TEST_P(json_mapping, returns_last_value_with_duplicate_warning) {
  omni::ryml::settings options;
  options.error_policy.duplicates =
    omni::json::duplicate_policy::last | omni::json::duplicate_policy::warn;
  const auto result =
    GetParam().deserialize(options, R"({"value": 7, "value": 42})");

  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_EQ(42, result.value->value);
  }
  EXPECT_EQ("/value: warning: duplicate field", result.diagnostics);
}

TEST_P(json_mapping, warn_alone_does_not_allow_unknown_fields) {
  omni::ryml::settings options;
  options.error_policy.unknown_fields = omni::json::unknown_policy::warn;
  const auto result =
    GetParam().deserialize(options, R"({"value": 7, "other": 42})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/other: unknown field", result.diagnostics);
}

TEST_P(json_mapping, returns_valid_value_with_unknown_warning) {
  omni::ryml::settings options;
  options.error_policy.unknown_fields =
    omni::json::unknown_policy::allow | omni::json::unknown_policy::warn;
  const auto result =
    GetParam().deserialize(options, R"({"value": 7, "other": 42})");

  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_EQ(7, result.value->value);
  }
  EXPECT_EQ("/other: warning: unknown field", result.diagnostics);
}

TEST_P(json_mapping, warning_limit_allows_exactly_the_limit) {
  omni::ryml::settings options;
  options.error_policy.unknown_fields =
    omni::json::unknown_policy::allow | omni::json::unknown_policy::warn;
  options.error_policy.warning_limit = 1;
  const auto result =
    GetParam().deserialize(options, R"({"value": 7, "other": 42})");

  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_EQ(7, result.value->value);
  }
  EXPECT_EQ("/other: warning: unknown field", result.diagnostics);
}

TEST_P(json_mapping, warning_limit_failure_returns_no_value) {
  omni::ryml::settings options;
  options.error_policy.unknown_fields =
    omni::json::unknown_policy::allow | omni::json::unknown_policy::warn;
  options.error_policy.warning_limit = 0;
  const auto result =
    GetParam().deserialize(options, R"({"value": 7, "other": 42})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/other: warning: unknown field\n/other: warning limit exceeded",
    result.diagnostics);
}

TEST_P(json_mapping, warning_limit_counts_across_policies) {
  omni::ryml::settings options;
  options.error_policy.duplicates =
    omni::json::duplicate_policy::allow | omni::json::duplicate_policy::warn;
  options.error_policy.unknown_fields =
    omni::json::unknown_policy::allow | omni::json::unknown_policy::warn;
  options.error_policy.warning_limit = 1;
  const auto result =
    GetParam().deserialize(options, R"({"value": 7, "value": 42, "other": 5})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(
    "/value: warning: duplicate field\n/other: warning: unknown field\n/other: warning limit exceeded",
    result.diagnostics);
}

TEST_P(json_mapping, missing_field_is_an_error_without_recovery) {
  omni::ryml::settings options;
  const auto result = GetParam().deserialize(options, R"({})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/value: missing field", result.diagnostics);
}

TEST_P(json_mapping, recovers_exposed_field_default) {
  omni::ryml::settings options;
  options.recovery = omni::json::recovery_policy::field_defaults;
  const auto result = GetParam().deserialize(options, R"({})");

  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_EQ(17, result.value->value);
  }
  EXPECT_EQ("", result.diagnostics);
}

TEST_P(json_mapping, value_initializes_missing_field) {
  omni::ryml::settings options;
  options.recovery = omni::json::recovery_policy::default_initializable;
  const auto result = GetParam().deserialize(options, R"({})");

  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_EQ(0, result.value->value);
  }
  EXPECT_EQ("", result.diagnostics);
}

TEST_P(json_mapping, field_default_takes_precedence) {
  omni::ryml::settings options;
  options.recovery = omni::json::recovery_policy::field_defaults
    | omni::json::recovery_policy::default_initializable;
  const auto result = GetParam().deserialize(options, R"({})");

  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_EQ(17, result.value->value);
  }
  EXPECT_EQ("", result.diagnostics);
}

TEST_P(json_mapping, recovery_does_not_accept_invalid_value) {
  omni::ryml::settings options;
  options.recovery = omni::json::recovery_policy::field_defaults
    | omni::json::recovery_policy::default_initializable;
  const auto result = GetParam().deserialize(options, R"({"value": "bad"})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/value: \"bad\" is not an integer", result.diagnostics);
}

TEST_P(json_mapping, recovery_does_not_accept_explicit_null) {
  omni::ryml::settings options;
  options.recovery = omni::json::recovery_policy::field_defaults;
  const auto result = GetParam().deserialize(options, R"({"value": null})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/value: \"null\" is not an integer", result.diagnostics);
}

TEST_P(json_mapping, recovery_does_not_spend_warning_limit) {
  omni::ryml::settings options;
  options.recovery = omni::json::recovery_policy::field_defaults;
  options.error_policy.warning_limit = 0;
  const auto result = GetParam().deserialize(options, R"({})");

  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_EQ(17, result.value->value);
  }
  EXPECT_EQ("", result.diagnostics);
}

struct optional_record {
  omni::compat::optional<record> nested;
};

TEST(json_mapping_optionals, warnings_do_not_discard_a_valid_optional_record) {
  const auto result =
    omni::ryml::deserialize.transform_settings([](omni::ryml::settings s) {
      s.error_policy.unknown_fields =
        omni::json::unknown_policy::allow | omni::json::unknown_policy::warn;
      return s;
    })(omni::type_t<optional_record>{},
      R"({"nested": {"value": 7, "other": 42}})");
  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_TRUE(result.value->nested);
    if (result.value->nested) {
      EXPECT_EQ(7, result.value->nested->value);
    }
  }
  EXPECT_EQ("/nested/other: warning: unknown field",
    omni::ryml::render_diagnostics(result.diagnostics));
}

// Probe the real method's substitution constraints without invoking mapping.
struct settings_transform_expression {
  template <typename F>
  auto operator()(F &&f) const
    -> decltype(omni::ryml::deserialize.transform_settings(std::forward<F>(f)));
};

struct wrong_settings_result {
  int operator()(omni::ryml::settings) const;
};

static_assert(
  !omni::compat::is_invocable<settings_transform_expression, int>::value,
  "settings transformation rejects non-callables");
static_assert(!omni::compat::is_invocable<settings_transform_expression,
                wrong_settings_result>::value,
  "settings transformation requires settings");

// C++11 callable equivalent of a capturing lambda; also checks constant
// evaluation when the optional implementation supports it in C++14 and newer.
struct set_error_limit {
  std::uint16_t limit;

  OMNI_CPP14_CONSTEXPR omni::ryml::settings operator()(
    omni::ryml::settings s) const {
    s.error_policy.error_limit = limit;
    return s;
  }
};

#if 201402L <= OMNI_CPLUSPLUS
constexpr auto configured =
  omni::ryml::deserialize.transform_settings(set_error_limit{3});
static_assert(3 == configured.settings.error_policy.error_limit,
  "settings transformation supports constant evaluation");
#endif

TEST(json_settings, captures_transform_a_copy_once) {
  int calls = 0;
  const std::uint16_t limit = 3;
  const auto configured = omni::ryml::deserialize.transform_settings(
    [&calls, limit](omni::ryml::settings s) {
      ++calls;
      s.error_policy.error_limit = limit;
      return s;
    });
  EXPECT_EQ(1, calls);
  EXPECT_EQ(3, configured.settings.error_policy.error_limit);
  EXPECT_EQ(0, omni::ryml::deserialize.settings.error_policy.error_limit);
}

int runtime_default() {
  return 23;
}

struct runtime_initialized_record {
  int value = runtime_default();
};

TEST(json_recovery, does_not_assume_every_initializer_is_exposed) {
  const auto result =
    omni::ryml::deserialize.transform_settings([](omni::ryml::settings s) {
      s.recovery = omni::json::recovery_policy::field_defaults;
      return s;
    })(omni::type_t<runtime_initialized_record>{}, "{}");
  EXPECT_FALSE(result.value);
  EXPECT_EQ("/value: missing field",
    omni::ryml::render_diagnostics(result.diagnostics));
}

INSTANTIATE_TEST_SUITE_P(ryml,
  json_mapping,
  testing::Values(engine{"ryml", deserialize_ryml}),
  [](const testing::TestParamInfo<engine> &p) { return p.param.name; });

} // namespace
