#include "serialization/data.hpp"
#include "serialization/deserialize.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <numeric>
#include <string>
#include <tuple>
#include <vector>

namespace {

constexpr omni::ryml::deserialize_t constant_deserialize{
  /*strategy=*/omni::ryml::use_tolerance(12).allow_partial(false),
};
static_assert(12 == constant_deserialize.strategy.tolerance,
  "configuration must remain usable in C++11 constant expressions");
static_assert(0 == omni::ryml::deserialize.strategy.tolerance,
  "the default deserializer must stop at the first error");
static_assert(!omni::ryml::deserialize.strategy.partial,
  "the default deserializer must reject values with field errors");

#if defined(__cpp_designated_initializers) \
  && 201707L <= __cpp_designated_initializers
constexpr omni::ryml::deserialize_t configured_deserialize{
  .strategy = omni::ryml::strategy{
    .tolerance = 0,
    .partial = false,
  },
};
static_assert(std::is_aggregate<omni::ryml::deserialize_t>::value,
  "deserializers must support aggregate configuration");
#endif

TEST(deserialization, maps_a_representative_document) {
  const auto result =
    omni::ryml::deserialize(omni::type_t<serialization_data::document>{},
      std::string{serialization_data::representative_json})
      .map_diagnostics(omni::ryml::render_diangostics);

  EXPECT_TRUE(result.value);
  if (!result.value)
    return;

  EXPECT_EQ("", result.diagnostics);
  EXPECT_EQ(3U, result.value->schema_version);
  EXPECT_EQ("checkout-api", result.value->source);
  ASSERT_EQ(3U, result.value->orders.size());
  EXPECT_EQ(8150001U, result.value->orders[0].id);
  EXPECT_EQ("oceanic-labs", result.value->orders[0].customer);
  EXPECT_TRUE(result.value->orders[0].expedited);
  EXPECT_EQ("Lisbon", result.value->orders[0].shipping.city);
  EXPECT_EQ("Rua do Oceano 815", result.value->orders[0].shipping.street);
  EXPECT_DOUBLE_EQ(38.7223,
    result.value->orders[0].shipping.location.latitude);
  EXPECT_DOUBLE_EQ(-9.1393,
    result.value->orders[0].shipping.location.longitude);
  ASSERT_EQ(3U, result.value->orders[0].items.size());
  EXPECT_EQ("sensor-815", result.value->orders[0].items[0].sku);
  EXPECT_EQ("Ocean sensor", result.value->orders[0].items[0].description);
  EXPECT_EQ(4U, result.value->orders[0].items[0].quantity);
  EXPECT_DOUBLE_EQ(42.5, result.value->orders[0].items[0].unit_price);
  EXPECT_TRUE(result.value->orders[0].items[0].taxable);
  EXPECT_EQ((std::vector<int>{8, 15, 42, 108}),
    result.value->orders[0].checkpoints);
  EXPECT_EQ(8150003U, result.value->orders[2].id);
  EXPECT_EQ("pelagic-systems", result.value->orders[2].customer);
}

TEST(deserialization, maps_the_same_schema_from_yaml) {
  const auto result =
    omni::ryml::deserialize(omni::type_t<serialization_data::document>{},
      std::string{serialization_data::representative_yaml})
      .map_diagnostics(omni::ryml::render_diangostics);

  EXPECT_TRUE(result.value);
  if (!result.value)
    return;

  EXPECT_EQ("", result.diagnostics);
  EXPECT_EQ(3U, result.value->schema_version);
  EXPECT_EQ("checkout-api", result.value->source);
  ASSERT_EQ(3U, result.value->orders.size());
  EXPECT_EQ("northwind-research", result.value->orders[1].customer);
  EXPECT_EQ("Reykjavik", result.value->orders[1].shipping.city);
  ASSERT_EQ(3U, result.value->orders[1].items.size());
  EXPECT_EQ("probe-42", result.value->orders[1].items[0].sku);
  EXPECT_DOUBLE_EQ(815.0, result.value->orders[1].items[0].unit_price);
}

TEST(deserialization, maps_scalars_sequences_and_nested_records) {
  const auto result =
    omni::ryml::deserialize(omni::type_t<serialization_data::scalar_values>{},
      std::string{serialization_data::scalar_json})
      .map_diagnostics(omni::ryml::render_diangostics);

  EXPECT_TRUE(result.value);
  if (!result.value)
    return;

  EXPECT_EQ("", result.diagnostics);
  EXPECT_TRUE(result.value->enabled);
  EXPECT_EQ(108U, result.value->retries);
  EXPECT_EQ(-42, result.value->delta);
  EXPECT_EQ((std::vector<int>{8, 15}), result.value->values);
  EXPECT_EQ("oceanic", result.value->label);
  ASSERT_EQ(2U, result.value->records.size());
  EXPECT_EQ("oceanic", result.value->records[0].name);
  EXPECT_EQ(815, result.value->records[0].code);
  EXPECT_EQ("sunset", result.value->records[1].name);
  EXPECT_EQ(108, result.value->records[1].code);
}

TEST(deserialization, assigns_bitfields_through_field_bindings) {
  const auto result =
    omni::ryml::deserialize(omni::type_t<serialization_data::bitfield_values>{},
      std::string{serialization_data::bitfield_json})
      .map_diagnostics(omni::ryml::render_diangostics);

  EXPECT_TRUE(result.value);
  if (!result.value)
    return;

  EXPECT_EQ("", result.diagnostics);
  EXPECT_EQ(815U, result.value->code);
  EXPECT_TRUE(result.value->enabled);
}

TEST(deserialization, reports_the_schema_scalar_expected_by_a_field) {
  const auto result =
    omni::ryml::deserialize(omni::type_t<serialization_data::nested_record>{},
      std::string{serialization_data::invalid_nested_integer_json})
      .map_diagnostics(omni::ryml::render_diangostics);

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/data/code: \"invalid\" is not an integer", result.diagnostics);
}

TEST(deserialization, rejects_numeric_boolean_values) {
  const auto result =
    omni::ryml::deserialize(omni::type_t<serialization_data::bitfield_values>{},
      std::string{R"({"code":815,"enabled":108})"})
      .map_diagnostics(omni::ryml::render_diangostics);

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/enabled: \"108\" is not a boolean", result.diagnostics);
}

TEST(deserialization, rejects_quoted_boolean_values) {
  const auto result =
    omni::ryml::deserialize(omni::type_t<serialization_data::bitfield_values>{},
      std::string{R"({"code":815,"enabled":"true"})"})
      .map_diagnostics(omni::ryml::render_diangostics);

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/enabled: \"true\" is not a boolean", result.diagnostics);
}

TEST(deserialization, partially_applies_strategy_and_destination) {
  const auto deserialize =
    omni::fn::partial(omni::ryml::deserialize_t{
                        /*strategy=*/omni::ryml::use_tolerance(0U),
                      },
      omni::type_t<serialization_data::bitfield_values>{});
  const auto result =
    deserialize(std::string{serialization_data::bitfield_json})
      .map_diagnostics(omni::ryml::render_diangostics);

  EXPECT_TRUE(result.value);
  if (!result.value)
    return;

  EXPECT_EQ("", result.diagnostics);
  EXPECT_EQ(815U, result.value->code);
  EXPECT_TRUE(result.value->enabled);
}

TEST(deserialization, rejects_unknown_input_fields_by_default) {
  const auto result =
    omni::ryml::deserialize(omni::type_t<serialization_data::nested_record>{},
      std::string{serialization_data::unknown_field_json})
      .map_diagnostics(omni::ryml::render_diangostics);

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/extra: unknown field", result.diagnostics);
}

TEST(deserialization, rejects_duplicate_input_fields_by_default) {
  const auto result =
    omni::ryml::deserialize(omni::type_t<serialization_data::nested_record>{},
      std::string{serialization_data::duplicate_field_json})
      .map_diagnostics(omni::ryml::render_diangostics);

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/ratio: duplicate field", result.diagnostics);
}

TEST(deserialization, rejects_missing_destination_fields_by_default) {
  const auto result =
    omni::ryml::deserialize(omni::type_t<serialization_data::nested_record>{},
      std::string{serialization_data::missing_field_json})
      .map_diagnostics(omni::ryml::render_diangostics);

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/data: missing field", result.diagnostics);
}

// Each document either supplies a payload code or a schema error.
struct mapping_case {
  std::string name;
  std::string source;
  tl::expected<int, std::string> expected_code;
};

class configured_deserialization:
    public testing::TestWithParam<mapping_case> {};

TEST_P(configured_deserialization, maps_or_reports_invalid_input) {
  const auto deserialize = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(0U),
  };
  const auto result =
    deserialize(omni::type_t<serialization_data::payload>{}, GetParam().source)
      .map_diagnostics(omni::ryml::render_diangostics);

  EXPECT_EQ(bool(GetParam().expected_code), bool(result.value));
  if (GetParam().expected_code && result.value) {
    EXPECT_EQ(*GetParam().expected_code, result.value->code);
    EXPECT_EQ("", result.diagnostics);
  } else if (!GetParam().expected_code) {
    EXPECT_EQ(GetParam().expected_code.error(), result.diagnostics);
  }
  EXPECT_EQ(0U, deserialize.strategy.tolerance);
}

#if defined(__cpp_designated_initializers) \
  && 201707L <= __cpp_designated_initializers
TEST_P(configured_deserialization,
  maps_with_designated_constant_configuration) {
  const auto result =
    configured_deserialize(omni::type_t<serialization_data::payload>{},
      GetParam().source)
      .map_diagnostics(omni::ryml::render_diangostics);

  EXPECT_EQ(bool(GetParam().expected_code), bool(result.value));
  if (GetParam().expected_code && result.value) {
    EXPECT_EQ(*GetParam().expected_code, result.value->code);
    EXPECT_EQ("", result.diagnostics);
  } else if (!GetParam().expected_code) {
    EXPECT_EQ(GetParam().expected_code.error(), result.diagnostics);
  }
}

TEST_P(configured_deserialization, maps_with_designated_runtime_configuration) {
  const unsigned tolerance = 0;
  const omni::ryml::deserialize_t deserialize{
    .strategy = omni::ryml::strategy{
      .tolerance = tolerance,
      .partial = false,
    },
  };
  const auto result =
    deserialize(omni::type_t<serialization_data::payload>{}, GetParam().source)
      .map_diagnostics(omni::ryml::render_diangostics);

  EXPECT_EQ(bool(GetParam().expected_code), bool(result.value));
  if (GetParam().expected_code && result.value) {
    EXPECT_EQ(*GetParam().expected_code, result.value->code);
    EXPECT_EQ("", result.diagnostics);
  } else if (!GetParam().expected_code) {
    EXPECT_EQ(GetParam().expected_code.error(), result.diagnostics);
  }
  EXPECT_EQ(tolerance, deserialize.strategy.tolerance);
}
#endif

INSTANTIATE_TEST_SUITE_P(configuration,
  configured_deserialization,
  testing::Values(
    mapping_case{"valid", R"({"name":"oceanic","code":815})", 815},
    mapping_case{"reordered", R"({"code":108,"name":"sunset"})", 108},
    mapping_case{"invalid_integer",
      R"({"name":"oceanic","code":"invalid"})",
      tl::make_unexpected(std::string{"/code: \"invalid\" is not an integer"})},
    mapping_case{"missing_field",
      R"({"name":"oceanic"})",
      tl::make_unexpected(std::string{"/code: missing field"})},
    mapping_case{"unknown_field",
      R"({"name":"oceanic","code":815,"extra":1})",
      tl::make_unexpected(std::string{"/extra: unknown field"})},
    mapping_case{"duplicate_field",
      R"({"name":"oceanic","code":815,"code":108})",
      tl::make_unexpected(std::string{"/code: duplicate field"})}),
  [](const testing::TestParamInfo<mapping_case> &p) { return p.param.name; });

constexpr auto updated_strategy =
  omni::ryml::use_tolerance(2).allow_partial(true).use_tolerance(3);
static_assert(3 == updated_strategy.tolerance && updated_strategy.partial,
  "builders must remain usable in C++11 constant expressions");
static_assert(!omni::ryml::strategy{}.partial,
  "the default strategy must reject values with field errors");

TEST(deserialization_strategy, changes_fields_without_changing_the_source) {
  const auto original = omni::ryml::use_tolerance(2).allow_partial(true);
  const auto updated = original.use_tolerance(5).allow_partial(false);

  EXPECT_EQ(2, original.tolerance);
  EXPECT_TRUE(original.partial);
  EXPECT_EQ(5, updated.tolerance);
  EXPECT_FALSE(updated.partial);
}

// Budget and partial policy vary independently for each input document.
struct diagnostics_case {
  std::string name;
  int tolerance;
  bool partial;
  std::string source;
  bool has_value;
  bool stopped;
  std::vector<std::string> messages;
  std::string mapped_name;
  int mapped_code;
};

class deserialization_diagnostics:
    public testing::TestWithParam<diagnostics_case> {};

TEST_P(deserialization_diagnostics,
  retains_diagnostics_and_applies_independent_policies) {
  const auto deserialize = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(GetParam().tolerance)
      .allow_partial(GetParam().partial),
  };
  const auto result =
    deserialize(omni::type_t<serialization_data::payload>{}, GetParam().source);

  EXPECT_EQ(GetParam().has_value, bool(result.value));
  EXPECT_EQ(GetParam().stopped, result.diagnostics.stopped());
  EXPECT_EQ(GetParam().messages.size(), result.diagnostics.issues.size());
  EXPECT_EQ(std::accumulate(GetParam().messages.begin(),
              GetParam().messages.end(),
              std::string{},
              [](std::string message, const std::string &entry) {
                return message.empty() ? entry : message + '\n' + entry;
              }),
    omni::ryml::render_diangostics(result.diagnostics));
  if (result.value) {
    EXPECT_EQ(GetParam().mapped_name, result.value->name);
    EXPECT_EQ(GetParam().mapped_code, result.value->code);
  }
}

TEST_P(deserialization_diagnostics, retains_policy_values_and_reports_partial) {
  const auto result = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(GetParam().tolerance)
      .allow_partial(GetParam().partial),
  }(omni::type_t<serialization_data::payload>{}, GetParam().source);

  EXPECT_EQ(0 >= GetParam().tolerance
      ? 0U
      : static_cast<std::size_t>(GetParam().tolerance),
    result.diagnostics.policy.tolerance);
  EXPECT_EQ(GetParam().partial, result.diagnostics.policy.partial);
  EXPECT_EQ(GetParam().has_value && !GetParam().messages.empty(),
    omni::ryml::is_partial(result));
}

TEST_P(deserialization_diagnostics,
  transforms_diagnostics_and_preserves_optional_value) {
  const auto result =
    omni::ryml::deserialize_t{
      /*strategy=*/omni::ryml::use_tolerance(GetParam().tolerance)
        .allow_partial(GetParam().partial),
    }(omni::type_t<serialization_data::payload>{}, GetParam().source)
      .map_diagnostics(omni::ryml::render_diangostics);

  EXPECT_EQ(GetParam().has_value, bool(result.value));
  EXPECT_EQ(std::accumulate(GetParam().messages.begin(),
              GetParam().messages.end(),
              std::string{},
              [](std::string message, const std::string &entry) {
                return message.empty() ? entry : message + '\n' + entry;
              }),
    result.diagnostics);
  if (result.value) {
    EXPECT_EQ(GetParam().mapped_name, result.value->name);
    EXPECT_EQ(GetParam().mapped_code, result.value->code);
  }
}

INSTANTIATE_TEST_SUITE_P(policies,
  deserialization_diagnostics,
  testing::Values( //
    diagnostics_case{"valid",
      0,
      false,
      R"({"name":"ok","code":7})",
      true,
      false,
      {},
      "ok",
      7},
    diagnostics_case{"first_error",
      0,
      false,
      R"({"name":{},"code":"bad"})",
      false,
      true,
      {"/name: expected string, found object"},
      "",
      0},
    diagnostics_case{"collect_without_value",
      2,
      false,
      R"({"name":{},"code":"bad"})",
      false,
      false,
      {"/name: expected string, found object",
        "/code: \"bad\" is not an integer"},
      "",
      0},
    diagnostics_case{"partial_first_error",
      0,
      true,
      R"({"name":"ok","code":"bad"})",
      true,
      true,
      {"/code: \"bad\" is not an integer"},
      "ok",
      0},
    diagnostics_case{"partial_budget_exhausted",
      1,
      true,
      R"({"name":{},"code":"bad"})",
      true,
      true,
      {"/name: expected string, found object",
        "/code: \"bad\" is not an integer"},
      "",
      0},
    diagnostics_case{"partial_budget_remaining",
      2,
      true,
      R"({"name":{},"code":"bad"})",
      true,
      false,
      {"/name: expected string, found object",
        "/code: \"bad\" is not an integer"},
      "",
      0},
    diagnostics_case{"unknown_field",
      1,
      true,
      R"({"name":"ok","code":7,"extra":1})",
      true,
      false,
      {"/extra: unknown field"},
      "ok",
      7},
    diagnostics_case{"duplicate_keeps_first",
      1,
      true,
      R"({"name":"ok","code":7,"code":8})",
      true,
      false,
      {"/code: duplicate field"},
      "ok",
      7},
    diagnostics_case{"missing_retains_default",
      1,
      true,
      R"({"name":"ok"})",
      true,
      false,
      {"/code: missing field"},
      "ok",
      0},
    diagnostics_case{"negative_budget_stops_first",
      -1,
      true,
      R"({"name":{},"code":"bad"})",
      true,
      true,
      {"/name: expected string, found object"},
      "",
      0},
    diagnostics_case{"wrong_root_shape",
      2,
      false,
      "[]",
      false,
      false,
      {"<root>: expected object, found array"},
      "",
      0}),
  [](const testing::TestParamInfo<diagnostics_case> &p) {
    return p.param.name;
  });

TEST(deserialization_diagnostics,
  records_nested_path_without_parser_source_positions) {
  const auto result =
    omni::ryml::deserialize(omni::type_t<serialization_data::nested_record>{},
      "ratio: 2.5\ndata:\n  name: ok\n  code: bad\n");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  const auto &d = result.diagnostics.issues.at(0);
  EXPECT_EQ(omni::ryml::issue::code::invalid_scalar, d.reason);
  EXPECT_EQ(omni::ryml::issue::kind::integer, d.expected);
  EXPECT_EQ(omni::ryml::issue::kind::scalar, d.actual);
  EXPECT_EQ(2U, d.path.size());
  EXPECT_EQ("data", omni::ryml::detail::to_string(d.path.at(0).field));
  EXPECT_EQ("code", omni::ryml::detail::to_string(d.path.at(1).field));
  EXPECT_EQ(tl::nullopt, d.path.at(1).index);
  EXPECT_EQ(std::string::npos, d.offset);
  EXPECT_EQ(std::string::npos, d.line);
  EXPECT_EQ(std::string::npos, d.column);
  EXPECT_EQ("bad",
    omni::ryml::detail::to_string(result.diagnostics.tree.val(d.node_id)));
}

TEST(deserialization_diagnostics, shares_one_budget_across_nested_sequences) {
  const auto result = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(1U).allow_partial(true),
  }(omni::type_t<serialization_data::scalar_values>{},
    R"({"enabled":true,"retries":2,"delta":3,"values":[1,"bad",3],
      "label":"ok","records":[{"name":"first","code":"bad"},
                              {"name":"unvisited","code":8}]})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.stopped());
  EXPECT_EQ(2U, result.diagnostics.issues.size());
  EXPECT_EQ(
    "/values/1: \"bad\" is not an integer\n"
    "/records/0/code: \"bad\" is not an integer",
    omni::ryml::render_diangostics(result.diagnostics));
  const auto &index = result.diagnostics.issues.at(0).path.at(1);
  EXPECT_EQ((tl::optional<std::size_t>{1U}), index.index);
  if (result.value) {
    EXPECT_EQ((std::vector<int>{1, 0, 3}), result.value->values);
    EXPECT_EQ(2U, result.value->records.size());
    EXPECT_EQ("first", result.value->records.at(0).name);
    EXPECT_EQ(0, result.value->records.at(0).code);
    EXPECT_EQ("", result.value->records.at(1).name);
    EXPECT_EQ(0, result.value->records.at(1).code);
  }
}

TEST(deserialization_diagnostics, retained_diagnostics_own_views_after_moves) {
  omni::ryml::diagnostics</*owning=*/ true> retained;
  {
    auto result =
      omni::ryml::deserialize(omni::type_t<serialization_data::payload>{},
        R"({"name":"ok","code":1,"some/unknown~field":2})");

    auto moved = std::move(result);
    retained = std::move(moved.diagnostics);
  }

  EXPECT_EQ("/some~1unknown~0field: unknown field",
    omni::ryml::render_diangostics(retained));
  EXPECT_EQ("some/unknown~field",
    omni::ryml::detail::to_string(retained.issues.at(0).path.at(0).field));
}

TEST(deserialization_diagnostics, moved_result_retains_value_and_scalar_views) {
  omni::ryml::with_diagnostics<serialization_data::payload> retained;
  {
    auto result = omni::ryml::deserialize_t{
      /*strategy=*/omni::ryml::use_tolerance(2U).allow_partial(true),
    }(omni::type_t<serialization_data::payload>{},
      R"({"name":"ok","code":"bad"})");

    retained = std::move(result);
  }

  EXPECT_TRUE(retained.value);
  EXPECT_EQ("/code: \"bad\" is not an integer",
    omni::ryml::render_diangostics(retained.diagnostics));
  if (retained.value) {
    EXPECT_EQ("ok", retained.value->name);
    EXPECT_EQ(0, retained.value->code);
  }
}

TEST(deserialization_diagnostics,
  diagnostics_transformation_keeps_partial_value_and_message) {
  auto result = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(2).allow_partial(true),
  }(omni::type_t<serialization_data::payload>{},
    R"({"name":"ok","code":"bad"})");

  EXPECT_EQ(1U, result.diagnostics.issues.size());
  const auto rendered =
    std::move(result).map_diagnostics(omni::ryml::render_diangostics);

  EXPECT_TRUE(rendered.value);
  if (rendered.value) {
    EXPECT_EQ("ok", rendered.value->name);
    EXPECT_EQ(0, rendered.value->code);
  }
  EXPECT_EQ("/code: \"bad\" is not an integer", rendered.diagnostics);
}

TEST(deserialization_diagnostics, valid_value_is_not_partial_when_permitted) {
  const auto result = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(3).allow_partial(true),
  }(omni::type_t<serialization_data::payload>{}, R"({"name":"ok","code":7})");

  EXPECT_EQ(3U, result.diagnostics.policy.tolerance);
  EXPECT_TRUE(result.diagnostics.policy.partial);
  EXPECT_TRUE(result.value);
  EXPECT_FALSE(omni::ryml::is_partial(result));
}

TEST(deserialization_diagnostics, moved_result_retains_policy_values) {
  omni::ryml::with_diagnostics<serialization_data::payload> retained;
  {
    auto result = omni::ryml::deserialize_t{
      /*strategy=*/omni::ryml::use_tolerance(2).allow_partial(true),
    }(omni::type_t<serialization_data::payload>{},
      R"({"name":"ok","code":"bad"})");

    retained = std::move(result);
  }

  EXPECT_EQ(2U, retained.diagnostics.policy.tolerance);
  EXPECT_TRUE(retained.diagnostics.policy.partial);
  EXPECT_TRUE(retained.value);
  EXPECT_TRUE(omni::ryml::is_partial(retained));
}

TEST(deserialization_diagnostics, parser_failure_precedes_mapping_policies) {
  const auto result = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(12U).allow_partial(true),
  }(omni::type_t<serialization_data::payload>{}, "name: [unterminated");

  EXPECT_FALSE(result.value);
  EXPECT_FALSE(omni::ryml::is_partial(result));
  EXPECT_EQ(12U, result.diagnostics.policy.tolerance);
  EXPECT_TRUE(result.diagnostics.policy.partial);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (1U == result.diagnostics.issues.size()) {
    EXPECT_EQ(omni::ryml::issue::code::parse_error,
      result.diagnostics.issues.front().reason);
  }
  EXPECT_FALSE(omni::ryml::render_diangostics(result.diagnostics).empty());
}

// JSON and YAML inputs with their expected default diagnostic text.
struct rendering_case {
  std::string name;
  std::string source;
  std::string message;
  // A syntax error prevents mapping and produces a parse_error issue.
  bool parse_failed;
};

class diagnostic_rendering: public testing::TestWithParam<rendering_case> {};

TEST_P(diagnostic_rendering, renders_paths_and_messages_on_error) {
  auto result = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(4U),
  }(omni::type_t<serialization_data::payload>{}, GetParam().source);

  EXPECT_FALSE(result.value);
  EXPECT_EQ(GetParam().parse_failed ? 1 : 0,
    std::count_if(result.diagnostics.issues.begin(),
      result.diagnostics.issues.end(),
      [](const omni::ryml::issue &e) {
        return omni::ryml::issue::code::parse_error == e.reason;
      }));

  const auto rendered =
    std::move(result).map_diagnostics(omni::ryml::render_diangostics);

  EXPECT_FALSE(rendered.value);
  EXPECT_EQ(GetParam().message, rendered.diagnostics);
}

INSTANTIATE_TEST_SUITE_P(documents,
  diagnostic_rendering,
  testing::Values( //
    rendering_case{"json_scalar",
      R"({"name":"ok","code":"bad"})",
      "/code: \"bad\" is not an integer",
      false},
    rendering_case{"yaml_scalar",
      "name: ok\ncode: bad\n",
      "/code: \"bad\" is not an integer",
      false},
    rendering_case{"escaped_field",
      R"({"name":"ok","code":7,"a/~1":0})",
      "/a~1~01: unknown field",
      false},
    rendering_case{"empty_field",
      R"({"name":"ok","code":7,"":0})",
      "/: unknown field",
      false},
    rendering_case{"missing_field",
      R"({"name":"ok"})",
      "/code: missing field",
      false},
    rendering_case{"multiple_errors",
      R"({"name":{},"code":"bad"})",
      "/name: expected string, found object\n/code: \"bad\" is not an integer",
      false},
    rendering_case{"root", "[]", "<root>: expected object, found array", false},
    rendering_case{"syntax",
      "name: [unterminated",
      "line 2, column 21: missing terminating ]",
      true}),
  [](const testing::TestParamInfo<rendering_case> &p) { return p.param.name; });

TEST(diagnostic_rendering, renders_array_positions_in_nested_paths) {
  const auto result =
    omni::ryml::deserialize(omni::type_t<serialization_data::scalar_values>{},
      R"({"enabled":true,"retries":1,"delta":0,"values":[],"label":"ok",
         "records":[{"name":"ok","code":1},{"name":"bad","code":"x"}]})")
      .map_diagnostics(omni::ryml::render_diangostics);

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/records/1/code: \"x\" is not an integer", result.diagnostics);
}

TEST(diagnostic_rendering, no_issues_render_as_an_empty_string) {
  const auto result =
    omni::ryml::deserialize(omni::type_t<serialization_data::payload>{},
      R"({"name":"ok","code":7})");

  EXPECT_TRUE(result.value);
  EXPECT_EQ("", omni::ryml::render_diangostics(result.diagnostics));
}

struct person {
  std::string name;
  int age;
};

TEST(deserialization, transforms_default_diagnostics_into_a_string) {
  const std::string json = R"({"name":"Ada","age":"bad"})";
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
  const auto result =
    omni::ryml::deserialize(omni::type<person>, json)
      .map_diagnostics(omni::ryml::render_diangostics);
#else
  const auto result =
    omni::ryml::deserialize(omni::type_t<person>{}, json)
      .map_diagnostics(omni::ryml::render_diangostics);
#endif

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/age: \"bad\" is not an integer", result.diagnostics);
}

TEST(deserialization, diagnostics_transformation_moves_an_available_value) {
  omni::ryml::with_diagnostics<std::unique_ptr<int>> result{
    /*value=*/omni::compat::make_unique<int>(42),
    /*diagnostics=*/{},
  };

  const auto transformed =
    std::move(result).map_diagnostics(omni::ryml::render_diangostics);

  EXPECT_TRUE(transformed.value);
  if (transformed.value) {
    EXPECT_TRUE(*transformed.value);
    if (*transformed.value) {
      EXPECT_EQ(42, **transformed.value);
    }
  }
  EXPECT_EQ("", transformed.diagnostics);
}

TEST(deserialization, transforms_move_only_diagnostics_repeatedly) {
  omni::ryml::with_diagnostics<std::unique_ptr<int>, std::unique_ptr<int>>
    result{
      /*value=*/omni::compat::make_unique<int>(42),
      /*diagnostics=*/omni::compat::make_unique<int>(7),
    };

  auto rendered = std::move(result).map_diagnostics(
    [](std::unique_ptr<int> d) {
      return omni::compat::make_unique<std::string>(std::to_string(*d));
    });

  EXPECT_TRUE(rendered.value);
  if (rendered.value) {
    EXPECT_TRUE(*rendered.value);
    if (*rendered.value) {
      EXPECT_EQ(42, **rendered.value);
    }
  }
  EXPECT_TRUE(rendered.diagnostics);
  if (rendered.diagnostics) {
    EXPECT_EQ("7", *rendered.diagnostics);
  }

  const auto measured = std::move(rendered).map_diagnostics(
    [](std::unique_ptr<std::string> d) { return d->size(); });

  EXPECT_TRUE(measured.value);
  if (measured.value) {
    EXPECT_TRUE(*measured.value);
    if (*measured.value) {
      EXPECT_EQ(42, **measured.value);
    }
  }
  EXPECT_EQ(1U, measured.diagnostics);
}

// Malformed documents exercise different parser exits, including stack growth.
struct parser_failure_case {
  std::string name;
  std::string source;
};

class parser_recovery: public testing::TestWithParam<parser_failure_case> {};

TEST_P(parser_recovery, retains_failure_and_allows_the_next_invocation) {
  const auto deserialize = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(12U).allow_partial(true),
  };
  omni::ryml::with_diagnostics<serialization_data::payload> retained{};
  {
    auto failed = deserialize(omni::type_t<serialization_data::payload>{},
      GetParam().source);
    retained = std::move(failed);
  }

  EXPECT_FALSE(retained.value);
  EXPECT_EQ(1U, retained.diagnostics.issues.size());
  if (1U == retained.diagnostics.issues.size()) {
    EXPECT_EQ(omni::ryml::issue::code::parse_error,
      retained.diagnostics.issues.front().reason);
  }
  EXPECT_FALSE(omni::ryml::render_diangostics(retained.diagnostics).empty());

  const auto recovered =
    deserialize(omni::type_t<serialization_data::payload>{},
      R"({"name":"ok","code":7})")
      .map_diagnostics(omni::ryml::render_diangostics);
  EXPECT_TRUE(recovered.value);
  if (recovered.value) {
    EXPECT_EQ("ok", recovered.value->name);
    EXPECT_EQ(7, recovered.value->code);
  }
  EXPECT_EQ("", recovered.diagnostics);
}

INSTANTIATE_TEST_SUITE_P(malformed_documents,
  parser_recovery,
  testing::Values( //
    parser_failure_case{"unterminated_sequence", "name: [open"},
    parser_failure_case{"unterminated_map", R"({"name":"ok","code":)"},
    parser_failure_case{"invalid_escape", R"({"name":"\q","code":7})"},
    parser_failure_case{"container_key", "{[a,b]: value}"},
    parser_failure_case{"nested_failure", std::string(128, '[') + '}'}),
  [](const testing::TestParamInfo<parser_failure_case> &p) {
    return p.param.name;
  });

TEST_P(parser_recovery, renders_parse_errors_with_the_standalone_parse_format) {
  const auto expected = omni::ryml::parse(GetParam().source);
  EXPECT_FALSE(expected);
  if (expected)
    return;

  omni::ryml::with_diagnostics<serialization_data::payload, std::string>
    retained{};
  {
    auto result = omni::ryml::deserialize(
      omni::type_t<serialization_data::payload>{}, GetParam().source);
    auto moved = std::move(result);
    retained =
      std::move(moved).map_diagnostics(omni::ryml::render_diangostics);
  }

  EXPECT_FALSE(retained.value);
  EXPECT_EQ(expected.error(), retained.diagnostics);
}

class parse_failure_policies:
    public testing::TestWithParam<std::tuple<int, bool>> {};

TEST_P(parse_failure_policies, preserves_policies_without_returning_a_value) {
  omni::compat::apply(
    [](int tolerance, bool partial) {
      const auto result = omni::ryml::deserialize_t{
        /*strategy=*/omni::ryml::use_tolerance(tolerance)
          .allow_partial(partial),
      }(omni::type_t<serialization_data::payload>{}, "name: [unterminated");

      EXPECT_FALSE(result.value);
      EXPECT_FALSE(omni::ryml::is_partial(result));
      EXPECT_EQ(0 >= tolerance ? 0U : static_cast<std::size_t>(tolerance),
        result.diagnostics.policy.tolerance);
      EXPECT_EQ(partial, result.diagnostics.policy.partial);
      EXPECT_TRUE(result.diagnostics.tree.empty());
      EXPECT_EQ(1U, result.diagnostics.issues.size());
      if (1U == result.diagnostics.issues.size()) {
        EXPECT_EQ(omni::ryml::issue::code::parse_error,
          result.diagnostics.issues.front().reason);
        EXPECT_EQ(::ryml::NONE, result.diagnostics.issues.front().node_id);
        EXPECT_TRUE(result.diagnostics.issues.front().path.empty());
      }
      EXPECT_EQ("line 2, column 21: missing terminating ]",
        omni::ryml::render_diangostics(result.diagnostics));
    },
    GetParam());
}

INSTANTIATE_TEST_SUITE_P(configuration,
  parse_failure_policies,
  testing::Combine(testing::Values(-1, 0, 2), testing::Bool()),
  [](const testing::TestParamInfo<std::tuple<int, bool>> &p) {
    return omni::compat::apply(
      [](int tolerance, bool partial) {
        return std::string{0 > tolerance
            ? "negative"
            : 0 == tolerance ? "zero" : "positive"}
          + (partial ? "_partial" : "_strict");
      },
      p.param);
  });

struct initialized_payload {
  std::string name = "default";
  int code = 42;
};

TEST(deserialization_diagnostics,
  failed_and_missing_fields_keep_initialized_defaults) {
  const auto result = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(2U).allow_partial(true),
  }(omni::type_t<initialized_payload>{}, R"({"code":"bad"})");

  EXPECT_TRUE(result.value);
  EXPECT_EQ(2U, result.diagnostics.issues.size());
  EXPECT_EQ("/name: missing field\n/code: \"bad\" is not an integer",
    omni::ryml::render_diangostics(result.diagnostics));
  if (result.value) {
    EXPECT_EQ("default", result.value->name);
    EXPECT_EQ(42, result.value->code);
  }
}

TEST(deserialization_diagnostics,
  parser_failure_never_produces_a_mapping_result) {
  const auto result = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(12U).allow_partial(true),
  }(omni::type_t<serialization_data::payload>{}, "name: [unterminated");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (1U == result.diagnostics.issues.size()) {
    EXPECT_EQ(omni::ryml::issue::code::parse_error,
      result.diagnostics.issues.front().reason);
  }
  EXPECT_EQ("line 2, column 21: missing terminating ]",
    omni::ryml::render_diangostics(result.diagnostics));
}

#if defined(__cpp_designated_initializers) \
  && 201707L <= __cpp_designated_initializers
TEST(deserialization_diagnostics,
  designated_configuration_keeps_partial_policy) {
  constexpr omni::ryml::deserialize_t configured{
    .strategy = omni::ryml::strategy{
      .tolerance = 2,
      .partial = true,
    },
  };
  const auto result = configured(omni::type_t<serialization_data::payload>{},
    R"({"name":"ok","code":"bad"})");

  EXPECT_TRUE(result.value);
  EXPECT_FALSE(result.diagnostics.stopped());
  EXPECT_EQ(1U, result.diagnostics.issues.size());
}
#endif

TEST(parsing, owns_text_and_decodes_scalars_after_source_destruction) {
  const auto result =
    omni::ryml::parse(std::string{"name: \"a\\nb\"\ncode: 7\n"});

  EXPECT_TRUE(result);
  if (!result)
    return;

  EXPECT_EQ("a\nb",
    omni::ryml::detail::to_string(result->crootref()["name"].val()));
  EXPECT_EQ("7",
    omni::ryml::detail::to_string(result->crootref()["code"].val()));
}

TEST(parsing, forwards_rapidyaml_options) {
  const std::string source = "[one,\n two]\n";
  const auto detected = omni::ryml::parse(source);
  const auto disabled =
    omni::ryml::parse(source, ::ryml::ParserOptions{}.detect_flow_ml(false));

  EXPECT_TRUE(detected);
  EXPECT_TRUE(disabled);
  if (!detected || !disabled)
    return;

  EXPECT_TRUE(detected->crootref().is_flow_ml());
  EXPECT_TRUE(disabled->crootref().is_flow_sl());
  EXPECT_EQ(2U, disabled->crootref().num_children());
}

TEST_P(parser_recovery, public_parse_retains_error_and_skips_transformation) {
  std::string retained;
  bool mapped = false;
  {
    auto result =
      omni::ryml::parse(GetParam().source).transform([&mapped](::ryml::Tree) {
        mapped = true;
        return 7;
      });

    EXPECT_FALSE(result);
    if (result)
      return;

    retained = std::move(result.error());
  }

  EXPECT_FALSE(mapped);
  EXPECT_FALSE(retained.empty());
  const auto recovered = omni::ryml::parse("code: 7\n");
  EXPECT_TRUE(recovered);
  if (recovered) {
    EXPECT_EQ("7",
      omni::ryml::detail::to_string(recovered->crootref()["code"].val()));
  }
}

TEST_P(configured_deserialization, public_mapper_accepts_an_independent_tree) {
  const auto map =
    omni::fn::partial(omni::ryml::map_tree_t{
                        /*strategy=*/omni::ryml::use_tolerance(0U),
                      },
      omni::type_t<serialization_data::payload>{});
  const auto result =
    map(::ryml::parse_in_arena(
          c4::csubstr{GetParam().source.data(), GetParam().source.size()}))
      .map_diagnostics(omni::ryml::render_diangostics);

  EXPECT_EQ(bool(GetParam().expected_code), bool(result.value));
  if (GetParam().expected_code && result.value) {
    EXPECT_EQ(*GetParam().expected_code, result.value->code);
    EXPECT_EQ("", result.diagnostics);
  } else if (!GetParam().expected_code) {
    EXPECT_EQ(GetParam().expected_code.error(), result.diagnostics);
  }
}

TEST_P(deserialization_diagnostics, public_mapper_applies_the_same_policies) {
  const auto map = omni::ryml::map_tree_t{
    /*strategy=*/omni::ryml::use_tolerance(GetParam().tolerance)
      .allow_partial(GetParam().partial),
  };
  const auto result = map(omni::type_t<serialization_data::payload>{},
    ::ryml::parse_in_arena(
      c4::csubstr{GetParam().source.data(), GetParam().source.size()}));

  EXPECT_EQ(GetParam().has_value, bool(result.value));
  EXPECT_EQ(GetParam().stopped, result.diagnostics.stopped());
  EXPECT_EQ(GetParam().partial, result.diagnostics.policy.partial);
  EXPECT_EQ(0 >= GetParam().tolerance
      ? 0U
      : static_cast<std::size_t>(GetParam().tolerance),
    result.diagnostics.policy.tolerance);
  EXPECT_EQ(GetParam().messages.size(), result.diagnostics.issues.size());
  EXPECT_EQ(std::accumulate(GetParam().messages.begin(),
              GetParam().messages.end(),
              std::string{},
              [](std::string message, const std::string &entry) {
                return message.empty() ? entry : message + '\n' + entry;
              }),
    omni::ryml::render_diangostics(result.diagnostics));
  if (result.value) {
    EXPECT_EQ(GetParam().mapped_name, result.value->name);
    EXPECT_EQ(GetParam().mapped_code, result.value->code);
  }
}

TEST_P(deserialization_diagnostics, borrowed_mapper_applies_the_same_policies) {
  const auto map = omni::fn::partial( //
    omni::ryml::map_tree_t{
      /*strategy=*/omni::ryml::use_tolerance(GetParam().tolerance)
        .allow_partial(GetParam().partial),
    },
    omni::type_t<serialization_data::payload>{});
  const auto tree = ::ryml::parse_in_arena(
    c4::csubstr{GetParam().source.data(), GetParam().source.size()});
  const omni::ryml::with_diagnostics<serialization_data::payload,
    omni::ryml::diagnostics</*owning=*/ false>> result = map(tree.crootref());

  EXPECT_EQ(GetParam().has_value, bool(result.value));
  EXPECT_EQ(GetParam().stopped, result.diagnostics.stopped());
  EXPECT_EQ(GetParam().partial, result.diagnostics.policy.partial);
  EXPECT_EQ(0 >= GetParam().tolerance
      ? 0U
      : static_cast<std::size_t>(GetParam().tolerance),
    result.diagnostics.policy.tolerance);
  EXPECT_EQ(GetParam().messages.size(), result.diagnostics.issues.size());
  EXPECT_EQ(&tree, result.diagnostics.tree.tree());
  EXPECT_EQ(GetParam().has_value && !GetParam().messages.empty(),
    omni::ryml::is_partial(result));
  EXPECT_EQ(std::accumulate(GetParam().messages.begin(),
              GetParam().messages.end(),
              std::string{},
              [](std::string message, const std::string &entry) {
                return message.empty() ? entry : message + '\n' + entry;
              }),
    omni::ryml::render_diangostics(result.diagnostics));
  if (result.value) {
    EXPECT_EQ(GetParam().mapped_name, result.value->name);
    EXPECT_EQ(GetParam().mapped_code, result.value->code);
  }
}

TEST_P(configured_deserialization, borrows_mutable_node_refs_and_renders) {
  auto tree = ::ryml::parse_in_arena(
    c4::csubstr{GetParam().source.data(), GetParam().source.size()});
  omni::ryml::with_diagnostics<serialization_data::payload,
    omni::ryml::diagnostics</*owning=*/ false>> mapped =
    omni::ryml::map_tree(omni::type_t<serialization_data::payload>{},
      tree.rootref());
  const auto result =
    std::move(mapped).map_diagnostics(omni::ryml::render_diangostics);

  EXPECT_EQ(bool(GetParam().expected_code), bool(result.value));
  if (GetParam().expected_code && result.value) {
    EXPECT_EQ(*GetParam().expected_code, result.value->code);
    EXPECT_EQ("", result.diagnostics);
  } else if (!GetParam().expected_code) {
    EXPECT_EQ(GetParam().expected_code.error(), result.diagnostics);
  }
}

TEST(tree_mapping, copies_lvalue_trees_and_keeps_diagnostic_views_alive) {
  omni::ryml::with_diagnostics<serialization_data::payload> retained{};
  {
    auto tree = ::ryml::parse_in_arena(R"({"name":"ok","code":"bad"})");
    retained = omni::ryml::map_tree(omni::type_t<serialization_data::payload>{},
      tree);

    EXPECT_EQ("bad",
      omni::ryml::detail::to_string(tree.crootref()["code"].val()));
  }

  EXPECT_FALSE(retained.value);
  EXPECT_EQ("/code: \"bad\" is not an integer",
    omni::ryml::render_diangostics(retained.diagnostics));
}

TEST(tree_mapping, copies_const_lvalue_trees) {
  const auto tree = ::ryml::parse_in_arena(R"({"name":"ok","code":7})");
  const omni::ryml::with_diagnostics<serialization_data::payload> result =
    omni::ryml::map_tree(omni::type_t<serialization_data::payload>{}, tree);

  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_EQ("ok", result.value->name);
    EXPECT_EQ(7, result.value->code);
  }
  EXPECT_EQ("7",
    omni::ryml::detail::to_string(tree.crootref()["code"].val()));
  EXPECT_EQ("", omni::ryml::render_diangostics(result.diagnostics));
}

TEST(tree_mapping, borrows_subtrees_and_retains_relative_paths) {
  const auto tree = ::ryml::parse_in_arena(
    R"({"payload":{"name":"ok","code":"bad"},"other":1})");
  const auto subtree = tree.crootref()["payload"];
  const auto result =
    omni::ryml::map_tree(omni::type_t<serialization_data::payload>{}, subtree);

  EXPECT_FALSE(result.value);
  EXPECT_EQ(&tree, result.diagnostics.tree.tree());
  EXPECT_EQ(subtree.id(), result.diagnostics.tree.id());
  EXPECT_EQ("/code: \"bad\" is not an integer",
    omni::ryml::render_diangostics(result.diagnostics));
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (1U == result.diagnostics.issues.size()) {
    EXPECT_EQ(subtree["code"].id(),
      result.diagnostics.issues.front().node_id);
  }
}

TEST(tree_mapping, rendered_borrowed_results_outlive_the_tree) {
  omni::ryml::with_diagnostics<serialization_data::payload, std::string>
    retained{};
  {
    const auto tree = ::ryml::parse_in_arena("name: \"a\\nb\"\ncode: bad\n");
    auto result = omni::ryml::map_tree_t{
      /*strategy=*/omni::ryml::use_tolerance(2).allow_partial(true),
    }(omni::type_t<serialization_data::payload>{}, tree.crootref());

    EXPECT_TRUE(omni::ryml::is_partial(result));
    retained =
      std::move(result).map_diagnostics(omni::ryml::render_diangostics);
  }

  EXPECT_TRUE(retained.value);
  if (retained.value) {
    EXPECT_EQ("a\nb", retained.value->name);
    EXPECT_EQ(0, retained.value->code);
  }
  EXPECT_EQ("/code: \"bad\" is not an integer", retained.diagnostics);
}

TEST(tree_mapping, borrowed_mapping_shares_one_budget_across_nested_sequences) {
  const auto tree = ::ryml::parse_in_arena(
    R"({"enabled":true,"retries":2,"delta":3,"values":[1,"bad",3],
      "label":"ok","records":[{"name":"first","code":"bad"},
                              {"name":"unvisited","code":8}]})");
  const auto result = omni::ryml::map_tree_t{
    /*strategy=*/omni::ryml::use_tolerance(1).allow_partial(true),
  }(omni::type_t<serialization_data::scalar_values>{}, tree.crootref());

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.stopped());
  EXPECT_EQ(2U, result.diagnostics.issues.size());
  EXPECT_EQ("/values/1: \"bad\" is not an integer\n"
            "/records/0/code: \"bad\" is not an integer",
    omni::ryml::render_diangostics(result.diagnostics));
  if (result.value) {
    EXPECT_EQ((std::vector<int>{1, 0, 3}), result.value->values);
    EXPECT_EQ(2U, result.value->records.size());
    if (2U == result.value->records.size()) {
      EXPECT_EQ("first", result.value->records[0].name);
      EXPECT_EQ(0, result.value->records[0].code);
      EXPECT_EQ("", result.value->records[1].name);
      EXPECT_EQ(0, result.value->records[1].code);
    }
  }
}

TEST(tree_mapping, reports_an_invalid_borrowed_node_as_a_mapping_error) {
  const auto result =
    omni::ryml::map_tree(omni::type_t<serialization_data::payload>{},
      ::ryml::ConstNodeRef{});

  EXPECT_FALSE(result.value);
  EXPECT_TRUE(result.diagnostics.tree.invalid());
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (1U == result.diagnostics.issues.size()) {
    EXPECT_EQ(omni::ryml::issue::code::unexpected_kind,
      result.diagnostics.issues.front().reason);
    EXPECT_EQ(omni::ryml::issue::kind::unknown,
      result.diagnostics.issues.front().actual);
  }
  EXPECT_EQ("<root>: expected object, found scalar",
    omni::ryml::render_diangostics(result.diagnostics));
}

TEST(tree_mapping, defaults_to_strict_mapping_and_owns_diagnostic_views) {
  omni::ryml::with_diagnostics<serialization_data::payload> retained;
  {
    auto tree = ::ryml::parse_in_arena(R"({"name":"ok","code":"bad"})");
    retained = omni::ryml::map_tree(omni::type_t<serialization_data::payload>{},
      std::move(tree));
  }

  EXPECT_FALSE(retained.value);
  EXPECT_EQ(0U, retained.diagnostics.policy.tolerance);
  EXPECT_FALSE(retained.diagnostics.policy.partial);
  EXPECT_EQ("/code: \"bad\" is not an integer",
    omni::ryml::render_diangostics(retained.diagnostics));
}

TEST(tree_mapping, reports_an_empty_tree_as_a_mapping_error) {
  const auto result =
    omni::ryml::map_tree(omni::type_t<serialization_data::payload>{},
      ::ryml::Tree{0, 0});

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (1U == result.diagnostics.issues.size()) {
    EXPECT_EQ(omni::ryml::issue::code::unexpected_kind,
      result.diagnostics.issues.front().reason);
    EXPECT_EQ(omni::ryml::issue::kind::mapping,
      result.diagnostics.issues.front().expected);
    EXPECT_EQ(omni::ryml::issue::kind::unknown,
      result.diagnostics.issues.front().actual);
  }
}

struct empty_record {};

TEST(tree_mapping, deserializes_a_record_with_no_fields) {
  const auto result =
    omni::ryml::deserialize(omni::type_t<empty_record>{}, "{}");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  EXPECT_FALSE(result.diagnostics.stopped());
  EXPECT_TRUE(result.diagnostics.tree.crootref().is_map());
}

TEST(deserialization, forwards_parser_options_before_mapping) {
  const auto result =
    omni::ryml::deserialize(omni::type_t<serialization_data::payload>{},
      "{name: ok,\n code: 7}\n",
      ::ryml::ParserOptions{}.detect_flow_ml(false));

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.tree.crootref().is_flow_sl());
  if (result.value) {
    EXPECT_EQ("ok", result.value->name);
    EXPECT_EQ(7, result.value->code);
  }
}

TEST(tree_mapping, composes_public_parse_with_a_constant_configuration) {
  constexpr auto map = omni::ryml::map_tree_t{
    /*strategy=*/omni::ryml::use_tolerance(2).allow_partial(true),
  };
  static_assert(2 == map.strategy.tolerance,
    "mapper configuration must remain a C++11 constant expression");
  const auto result = omni::ryml::parse(R"({"name":"ok","code":"bad"})")
                        .transform(omni::fn::partial(map,
                          omni::type_t<serialization_data::payload>{}));

  EXPECT_TRUE(result);
  if (!result)
    return;

  EXPECT_TRUE(omni::ryml::is_partial(*result));
  EXPECT_EQ("/code: \"bad\" is not an integer",
    omni::ryml::render_diangostics(result->diagnostics));
  if (result->value) {
    EXPECT_EQ("ok", result->value->name);
    EXPECT_EQ(0, result->value->code);
  }
}

#if defined(__cpp_designated_initializers) \
  && 201707L <= __cpp_designated_initializers
TEST(tree_mapping, supports_designated_configuration) {
  constexpr omni::ryml::map_tree_t configured{
    .strategy =
      omni::ryml::strategy{
        .tolerance = 2,
        .partial = true,
      },
  };
  const auto result = configured(omni::type_t<serialization_data::payload>{},
    ::ryml::parse_in_arena(R"({"name":"ok","code":"bad"})"));

  EXPECT_TRUE(omni::ryml::is_partial(result));
  EXPECT_EQ(2U, result.diagnostics.policy.tolerance);
  EXPECT_EQ("/code: \"bad\" is not an integer",
    omni::ryml::render_diangostics(result.diagnostics));
}
#endif

TEST(parsing, unresolved_tag_returns_an_owned_error) {
  const auto result = omni::ryml::parse("!missing!kind value",
    ::ryml::ParserOptions{}.resolve_tags(true));

  EXPECT_FALSE(result);
  if (!result) {
    EXPECT_FALSE(result.error().empty());
  }

  const auto recovered = omni::ryml::parse("code: 7");
  EXPECT_TRUE(recovered);
  if (recovered) {
    EXPECT_EQ("7",
      omni::ryml::detail::to_string(recovered->crootref()["code"].val()));
  }
}

} // namespace
