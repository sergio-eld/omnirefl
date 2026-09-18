#include "data.hpp"

#include <omnirefl/serialization/ryml.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace {

TEST(serialization_formats, emits_json_without_configuration) {
  const auto output =
    omni::ryml::as_json(serialization_data::payload{"Ada", 815});
  EXPECT_FALSE(output.empty());
  if (output.empty())
    return;

  EXPECT_EQ('{', output.front());

  const auto restored =
    omni::ryml::deserialize(omni::type_t<serialization_data::payload>{},
      output);
  EXPECT_TRUE(restored.value);
  EXPECT_TRUE(restored.diagnostics.issues.empty());
  if (restored.value) {
    EXPECT_EQ("Ada", restored.value->name);
    EXPECT_EQ(815, restored.value->code);
  }
}

TEST(serialization_formats, emits_yaml_without_configuration) {
  const auto output =
    omni::ryml::as_yaml(serialization_data::payload{"Ada", 815});
  EXPECT_EQ(0U, output.find("name:"));

  const auto restored =
    omni::ryml::deserialize(omni::type_t<serialization_data::payload>{},
      output);
  EXPECT_TRUE(restored.value);
  EXPECT_TRUE(restored.diagnostics.issues.empty());
  if (restored.value) {
    EXPECT_EQ("Ada", restored.value->name);
    EXPECT_EQ(815, restored.value->code);
  }
}

TEST(deserialization, accepts_extra_fields_without_partial_values) {
  constexpr omni::ryml::deserialize_t deserialize{
    /*strategy=*/omni::ryml::use_tolerance(0).allow_extra(true),
  };
  static_assert(deserialize.strategy.extra,
    "extra fields must be configurable in C++11 constant expressions");

  const auto result = deserialize(omni::type_t<serialization_data::payload>{},
    R"({"name":"Ada","code":815,"extra":{"anything":[1,null,{}]}})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.policy.extra);
  EXPECT_FALSE(result.diagnostics.policy.partial);
  EXPECT_EQ(0U, result.diagnostics.policy.tolerance);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  EXPECT_FALSE(result.diagnostics.stopped());
  EXPECT_FALSE(omni::ryml::is_partial(result));
  EXPECT_EQ("", omni::ryml::render_diangostics(result.diagnostics));
  if (result.value) {
    EXPECT_EQ("Ada", result.value->name);
    EXPECT_EQ(815, result.value->code);
  }
}

struct person {
  // Person's display name.
  std::string name;
  // Person's age in years.
  int age;
};

TEST(deserialization, transforms_default_diagnostics_into_a_string) {
  const std::string json = R"({"name":"Ada","age":"bad"})";
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
  const auto result = omni::ryml::deserialize(omni::type<person>, json)
                        .map_diagnostics(omni::ryml::render_diangostics);
#else
  const auto result = omni::ryml::deserialize(omni::type_t<person>{}, json)
                        .map_diagnostics(omni::ryml::render_diangostics);
#endif

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/age: \"bad\" is not an integer", result.diagnostics);
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

TEST(deserialization, partially_applies_strategy_and_destination) {
  const auto deserialize =
    omni::fn::partial(omni::ryml::deserialize_t{
                        /*strategy=*/omni::ryml::use_tolerance(0),
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
  EXPECT_EQ(2U, result.value->records.size());
  if (2U != result.value->records.size())
    return;
  EXPECT_EQ("oceanic", result.value->records[0].name);
  EXPECT_EQ(815, result.value->records[0].code);
  EXPECT_EQ("sunset", result.value->records[1].name);
  EXPECT_EQ(108, result.value->records[1].code);
}

struct container_config {
  // Fallback for a missing or wrong-kind container.
  std::vector<int> values{9};
  // Shows whether traversal continued beyond the container.
  int following = 42;
};

TEST(deserialization_warnings, containers_continue_without_spending_tolerance) {
  const auto result = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(0).allow_partial(true),
  }(omni::type_t<container_config>{},
    R"({"values":[1,"bad",{},4],"following":7})");

  EXPECT_TRUE(result.value);
  EXPECT_FALSE(result.diagnostics.stopped());
  EXPECT_EQ(2U, result.diagnostics.issues.size());
  for (const auto &e : result.diagnostics.issues) {
    SCOPED_TRACE(e.node_id);
    EXPECT_TRUE(e.warning);
  }
  EXPECT_EQ(
    "/values/1: warning: \"bad\" is not an integer\n"
    "/values/2: warning: expected integer, found object",
    omni::ryml::render_diangostics(result.diagnostics));
  if (result.value) {
    EXPECT_EQ((std::vector<int>{1, 0, 0, 4}), result.value->values);
    EXPECT_EQ(7, result.value->following);
  }
}

class serialization: public testing::TestWithParam<::ryml::EmitType_e> {};

TEST_P(serialization, writes_record_fields_with_their_document_types) {
  const serialization_data::payload value{"Ada", 815};
  const auto output = omni::ryml::serialize_t{
    /*format=*/GetParam(),
  }(value);
  const auto parsed = omni::ryml::parse(output);

  EXPECT_TRUE(parsed);
  if (!parsed)
    return;

  const auto root = parsed->crootref();
  EXPECT_TRUE(root.is_map());
  if (!root.is_map())
    return;

  EXPECT_EQ(2U, root.num_children());
  EXPECT_TRUE(root.has_child("name"));
  EXPECT_TRUE(root.has_child("code"));
  if (!root.has_child("name") || !root.has_child("code"))
    return;

  EXPECT_EQ("Ada", root["name"].val());
  EXPECT_TRUE(root["name"].is_val_quoted());
  EXPECT_EQ("815", root["code"].val());
  EXPECT_FALSE(root["code"].is_val_quoted());
  if (::ryml::EMIT_JSON == GetParam()) {
    EXPECT_FALSE(output.empty());
    if (!output.empty()) {
      EXPECT_EQ('{', output.front());
    }
  } else {
    EXPECT_EQ(0U, output.find("name:"));
  }
}

TEST_P(serialization, writes_nested_records_and_sequences) {
  const serialization_data::scalar_values value{
    true,
    108,
    -42,
    {8, 15},
    "oceanic",
    {{"Ada", 815}, {"Grace", 108}},
  };
  const auto output = omni::ryml::serialize_t{
    /*format=*/GetParam(),
  }(value);
  const auto parsed = omni::ryml::parse(output);

  EXPECT_TRUE(parsed);
  if (!parsed)
    return;

  const auto root = parsed->crootref();
  EXPECT_TRUE(root.is_map());
  if (!root.is_map())
    return;

  EXPECT_EQ(6U, root.num_children());
  for (const auto name : std::initializer_list<c4::csubstr>{"enabled",
         "retries",
         "delta",
         "values",
         "label",
         "records"}) {
    SCOPED_TRACE(omni::ryml::detail::to_string(name));
    EXPECT_TRUE(root.has_child(name));
    if (!root.has_child(name))
      return;
  }

  EXPECT_EQ("true", root["enabled"].val());
  EXPECT_FALSE(root["enabled"].is_val_quoted());
  EXPECT_EQ("108", root["retries"].val());
  EXPECT_EQ("-42", root["delta"].val());
  EXPECT_TRUE(root["values"].is_seq());
  EXPECT_EQ(2U, root["values"].num_children());
  if (!root["values"].is_seq() || 2U != root["values"].num_children())
    return;

  EXPECT_EQ("8", root["values"][0].val());
  EXPECT_EQ("15", root["values"][1].val());
  EXPECT_EQ("oceanic", root["label"].val());
  EXPECT_TRUE(root["records"].is_seq());
  EXPECT_EQ(2U, root["records"].num_children());
  if (!root["records"].is_seq() || 2U != root["records"].num_children())
    return;

  for (const auto record : root["records"].children()) {
    SCOPED_TRACE(record.id());
    EXPECT_TRUE(record.is_map());
    if (!record.is_map())
      return;

    EXPECT_TRUE(record.has_child("name"));
    EXPECT_TRUE(record.has_child("code"));
    if (!record.has_child("name") || !record.has_child("code"))
      return;
  }

  EXPECT_TRUE(root["records"][0].is_map());
  EXPECT_EQ("Ada", root["records"][0]["name"].val());
  EXPECT_EQ("815", root["records"][0]["code"].val());
  EXPECT_EQ("Grace", root["records"][1]["name"].val());
  EXPECT_EQ("108", root["records"][1]["code"].val());
}

constexpr omni::ryml::deserialize_t constant_deserialize{
  /*strategy=*/omni::ryml::use_tolerance(12).allow_partial(false),
};
static_assert(12 == constant_deserialize.strategy.tolerance,
  "configuration must remain usable in C++11 constant expressions");
static_assert(0 == omni::ryml::deserialize.strategy.tolerance,
  "the default deserializer must stop at the first error");
static_assert(!omni::ryml::deserialize.strategy.partial,
  "the default deserializer must reject values with field errors");
static_assert(!omni::ryml::deserialize.strategy.extra,
  "the default deserializer must reject extra fields");
static_assert(!omni::ryml::map_tree.strategy.extra,
  "the default mapper must reject extra fields");
static_assert(!omni::ryml::strategy{}.extra,
  "value-initialized strategies must reject extra fields");

#if defined(__cpp_designated_initializers) \
  && 201707L <= __cpp_designated_initializers
constexpr omni::ryml::deserialize_t configured_deserialize{
  .strategy =
    omni::ryml::strategy{
      .tolerance = 0,
      .partial = false,
      .extra = false,
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
  EXPECT_EQ(3U, result.value->orders.size());
  if (3U != result.value->orders.size())
    return;
  EXPECT_EQ(8150001U, result.value->orders[0].id);
  EXPECT_EQ("oceanic-labs", result.value->orders[0].customer);
  EXPECT_TRUE(result.value->orders[0].expedited);
  EXPECT_EQ("Lisbon", result.value->orders[0].shipping.city);
  EXPECT_EQ("Rua do Oceano 815", result.value->orders[0].shipping.street);
  EXPECT_DOUBLE_EQ(38.7223, result.value->orders[0].shipping.location.latitude);
  EXPECT_DOUBLE_EQ(-9.1393,
    result.value->orders[0].shipping.location.longitude);
  EXPECT_EQ(3U, result.value->orders[0].items.size());
  if (3U != result.value->orders[0].items.size())
    return;
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
  EXPECT_EQ(3U, result.value->orders.size());
  if (3U != result.value->orders.size())
    return;
  EXPECT_EQ("northwind-research", result.value->orders[1].customer);
  EXPECT_EQ("Reykjavik", result.value->orders[1].shipping.city);
  EXPECT_EQ(3U, result.value->orders[1].items.size());
  if (3U != result.value->orders[1].items.size())
    return;
  EXPECT_EQ("probe-42", result.value->orders[1].items[0].sku);
  EXPECT_DOUBLE_EQ(815.0, result.value->orders[1].items[0].unit_price);
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
  // Label used in the individual test name.
  std::string name;
  // Document supplied to the deserializer or mapper.
  std::string source;
  // Expected payload code, or the default diagnostic text.
  omni::compat::expected<int, std::string> expected_code;
};

class configured_deserialization:
    public testing::TestWithParam<mapping_case> {};

TEST_P(configured_deserialization, maps_or_reports_invalid_input) {
  const auto deserialize = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(0),
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
  EXPECT_EQ(0, deserialize.strategy.tolerance);
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
  const int tolerance = 0;
  const omni::ryml::deserialize_t deserialize{
    .strategy =
      omni::ryml::strategy{
        .tolerance = tolerance,
        .partial = false,
        .extra = false,
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
      omni::compat::unexpected<std::string>{
        "/code: \"invalid\" is not an integer"}},
    mapping_case{"missing_field",
      R"({"name":"oceanic"})",
      omni::compat::unexpected<std::string>{"/code: missing field"}},
    mapping_case{"unknown_field",
      R"({"name":"oceanic","code":815,"extra":1})",
      omni::compat::unexpected<std::string>{"/extra: unknown field"}},
    mapping_case{"duplicate_field",
      R"({"name":"oceanic","code":815,"code":108})",
      omni::compat::unexpected<std::string>{"/code: duplicate field"}}),
  [](const testing::TestParamInfo<mapping_case> &p) { return p.param.name; });

// Unknown input fields, crossed with runtime extra and partial settings.
struct extra_fields_case {
  // Label used in the individual test name.
  std::string name;
  // Document containing a nested record and fields outside the model.
  std::string source;
  // First diagnostic when extra fields are rejected with zero tolerance.
  std::string message;
};

class extra_fields_policy:
    public testing::TestWithParam<std::tuple<extra_fields_case, bool, bool>> {};

TEST_P(extra_fields_policy, applies_extra_independently_of_partial) {
  omni::compat::apply(
    [](const extra_fields_case &c, bool extra, bool partial) {
      const auto result = omni::ryml::deserialize_t{
        /*strategy=*/omni::ryml::use_tolerance(0)
          .allow_extra(extra)
          .allow_partial(partial),
      }(omni::type_t<serialization_data::nested_record>{}, c.source);

      EXPECT_EQ(extra || partial, bool(result.value));
      EXPECT_EQ(extra, result.diagnostics.policy.extra);
      EXPECT_EQ(partial, result.diagnostics.policy.partial);
      EXPECT_EQ(0U, result.diagnostics.policy.tolerance);
      EXPECT_EQ(!extra, result.diagnostics.stopped());
      EXPECT_EQ(extra ? 0U : 1U, result.diagnostics.issues.size());
      EXPECT_EQ(extra ? std::string{} : c.message,
        omni::ryml::render_diangostics(result.diagnostics));
      if (!result.diagnostics.issues.empty()) {
        EXPECT_EQ(omni::ryml::issue::code::unknown_field,
          result.diagnostics.issues.front().reason);
        EXPECT_FALSE(result.diagnostics.issues.front().warning);
      }
      if (extra && result.value) {
        EXPECT_DOUBLE_EQ(2.5, result.value->ratio);
        EXPECT_EQ("Ada", result.value->data.name);
        EXPECT_EQ(815, result.value->data.code);
        EXPECT_FALSE(omni::ryml::is_partial(result));
      }
    },
    GetParam());
}

INSTANTIATE_TEST_SUITE_P(settings,
  extra_fields_policy,
  testing::Combine(
    testing::Values( //
      extra_fields_case{"root",
        R"({"extra":{},"ratio":2.5,"data":{"name":"Ada","code":815}})",
        "/extra: unknown field"},
      extra_fields_case{"nested",
        R"({"ratio":2.5,"data":{"name":"Ada","extra":[],"code":815}})",
        "/data/extra: unknown field"},
      extra_fields_case{"multiple",
        R"({"extra":1,"other":null,"ratio":2.5,
          "data":{"name":"Ada","code":815,"more":[{},false]}})",
        "/extra: unknown field"},
      extra_fields_case{"duplicate_extra",
        R"({"extra":1,"extra":2,"ratio":2.5,
          "data":{"name":"Ada","code":815}})",
        "/extra: unknown field"},
      extra_fields_case{"yaml",
        "ratio: 2.5\ndata:\n  name: Ada\n  code: 815\n  extra: [1, null]\n",
        "/data/extra: unknown field"}),
    testing::Bool(),
    testing::Bool()),
  [](const testing::TestParamInfo< //
    std::tuple<extra_fields_case, bool, bool>> &p) {
    return omni::compat::apply(
      [](const extra_fields_case &c, bool extra, bool partial) {
        return c.name + (extra ? "_allowed" : "_rejected")
          + (partial ? "_partial" : "_strict");
      },
      p.param);
  });

class allowed_extra_fields: public testing::TestWithParam<mapping_case> {};

TEST_P(allowed_extra_fields, validates_model_fields) {
  const auto result = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(0).allow_extra(true),
  }(omni::type_t<serialization_data::payload>{}, GetParam().source)
                        .map_diagnostics(omni::ryml::render_diangostics);

  EXPECT_EQ(bool(GetParam().expected_code), bool(result.value));
  if (GetParam().expected_code && result.value) {
    EXPECT_EQ(*GetParam().expected_code, result.value->code);
    EXPECT_EQ("", result.diagnostics);
  } else if (!GetParam().expected_code) {
    EXPECT_EQ(GetParam().expected_code.error(), result.diagnostics);
  }
}

INSTANTIATE_TEST_SUITE_P(documents,
  allowed_extra_fields,
  testing::Values( //
    mapping_case{"valid", R"({"extra":{},"name":"Ada","code":815})", 815},
    mapping_case{"duplicate_model_field",
      R"({"extra":{},"name":"Ada","code":815,"code":108})",
      omni::compat::unexpected<std::string>{"/code: duplicate field"}},
    mapping_case{"missing_model_field",
      R"({"extra":{},"name":"Ada"})",
      omni::compat::unexpected<std::string>{"/code: missing field"}},
    mapping_case{"invalid_model_scalar",
      R"({"extra":{},"name":"Ada","code":"bad"})",
      omni::compat::unexpected<std::string>{
        "/code: \"bad\" is not an integer"}},
    mapping_case{"wrong_root",
      "[]",
      omni::compat::unexpected<std::string>{
        "<root>: expected object, found array"}}),
  [](const testing::TestParamInfo<mapping_case> &p) { return p.param.name; });

struct nested_extra_config {
  // Required record whose extra fields should be ignored.
  serialization_data::payload object;
  // Record elements whose extra fields should be ignored.
  std::vector<serialization_data::payload> records;
  // Unknown fields must not prevent accepting this optional record.
  omni::compat::optional<serialization_data::payload> optional;
};

class nested_extra_fields: public testing::TestWithParam<bool> {};

TEST_P(nested_extra_fields, ignores_extra_fields_in_records_and_containers) {
  const auto result = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(0)
      .allow_partial(GetParam())
      .allow_extra(true),
  }(omni::type_t<nested_extra_config>{},
    R"({"extra":null,
      "object":{"name":"Ada","code":815,"extra":{}},
      "records":[{"name":"Grace","code":108,"extra":[]},
                 {"name":"Linus","code":42,"extra":false}],
      "optional":{"name":"Bjarne","code":23,"extra":{"more":1}}})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  EXPECT_FALSE(result.diagnostics.stopped());
  EXPECT_TRUE(result.diagnostics.policy.extra);
  EXPECT_EQ(GetParam(), result.diagnostics.policy.partial);
  EXPECT_FALSE(omni::ryml::is_partial(result));
  if (!result.value)
    return;

  EXPECT_EQ("Ada", result.value->object.name);
  EXPECT_EQ(815, result.value->object.code);
  EXPECT_EQ(2U, result.value->records.size());
  if (2U == result.value->records.size()) {
    EXPECT_EQ("Grace", result.value->records[0].name);
    EXPECT_EQ(108, result.value->records[0].code);
    EXPECT_EQ("Linus", result.value->records[1].name);
    EXPECT_EQ(42, result.value->records[1].code);
  }
  EXPECT_TRUE(result.value->optional);
  if (result.value->optional) {
    EXPECT_EQ("Bjarne", result.value->optional->name);
    EXPECT_EQ(23, result.value->optional->code);
  }
}

INSTANTIATE_TEST_SUITE_P(policies,
  nested_extra_fields,
  testing::Bool(),
  [](const testing::TestParamInfo<bool> &p) {
    return p.param ? "partial" : "strict";
  });

TEST(tree_mapping, borrowed_mapping_accepts_extra_fields) {
  const auto tree =
    ::ryml::parse_in_arena(R"({"extra":[1,{}],"name":"Ada","code":815})");
  const auto result = omni::ryml::map_tree_t{
    /*strategy=*/omni::ryml::use_tolerance(0).allow_extra(true),
  }(omni::type_t<serialization_data::payload>{}, tree.crootref());

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.policy.extra);
  EXPECT_FALSE(result.diagnostics.policy.partial);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  EXPECT_FALSE(result.diagnostics.stopped());
  EXPECT_EQ(&tree, result.diagnostics.tree.tree());
  if (result.value) {
    EXPECT_EQ("Ada", result.value->name);
    EXPECT_EQ(815, result.value->code);
  }
}

TEST(deserialization, extra_fields_do_not_hide_syntax_errors) {
  const auto result = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(0).allow_extra(true),
  }(omni::type_t<serialization_data::payload>{},
    R"({"name":"Ada","code":815,"extra":[)");

  EXPECT_FALSE(result.value);
  EXPECT_TRUE(result.diagnostics.policy.extra);
  EXPECT_FALSE(result.diagnostics.policy.partial);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (1U == result.diagnostics.issues.size()) {
    EXPECT_EQ(omni::ryml::issue::code::parse_error,
      result.diagnostics.issues.front().reason);
    EXPECT_FALSE(result.diagnostics.issues.front().warning);
  }
  EXPECT_FALSE(omni::ryml::render_diangostics(result.diagnostics).empty());
}

TEST(deserialization_strategy, builders_preserve_extra_and_other_settings) {
  constexpr auto original =
    omni::ryml::use_tolerance(2).allow_partial(true).allow_extra(true);
  constexpr auto rebound = original.use_tolerance(3).allow_partial(false);
  constexpr auto disabled = rebound.allow_extra(false);
  static_assert(rebound.extra,
    "tolerance and partial builders must preserve extra");
  static_assert(3 == disabled.tolerance,
    "allow_extra must preserve tolerance in constant expressions");
  static_assert(!disabled.partial,
    "allow_extra must preserve partial in constant expressions");
  static_assert(!disabled.extra,
    "allow_extra must be reversible in constant expressions");

  EXPECT_EQ(2, original.tolerance);
  EXPECT_TRUE(original.partial);
  EXPECT_TRUE(original.extra);
  EXPECT_EQ(3, disabled.tolerance);
  EXPECT_FALSE(disabled.partial);
  EXPECT_FALSE(disabled.extra);
}

#if defined(__cpp_designated_initializers) \
  && 201707L <= __cpp_designated_initializers
TEST(deserialization_strategy, designated_configuration_accepts_extra_fields) {
  constexpr omni::ryml::deserialize_t deserialize{
    .strategy =
      omni::ryml::strategy{
        .tolerance = 0,
        .partial = false,
        .extra = true,
      },
  };
  const auto result = deserialize(omni::type_t<serialization_data::payload>{},
    R"({"name":"Ada","code":815,"extra":null})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.policy.extra);
  EXPECT_FALSE(result.diagnostics.policy.partial);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_EQ("Ada", result.value->name);
    EXPECT_EQ(815, result.value->code);
  }
}
#endif

constexpr auto updated_strategy =
  omni::ryml::use_tolerance(2).allow_partial(true).use_tolerance(3);
static_assert(3 == updated_strategy.tolerance,
  "tolerance builders must remain usable in C++11 constant expressions");
static_assert(updated_strategy.partial,
  "rebinding tolerance must preserve the partial policy");
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
  // Label used in the individual test name.
  std::string name;
  // Number of hard errors traversal may pass.
  int tolerance;
  // Whether field errors permit returning the value.
  bool partial;
  // Document supplied to the deserializer or mapper.
  std::string source;
  // Whether a deserialized value should be returned.
  bool has_value;
  // Whether the hard-error budget stopped traversal.
  bool stopped;
  // Number of issues expected before traversal ends.
  std::size_t issue_count;
  // Expected default diagnostic text in traversal order.
  std::string message;
  // Expected name when a value is returned.
  std::string mapped_name;
  // Expected code when a value is returned.
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
  EXPECT_EQ(GetParam().issue_count, result.diagnostics.issues.size());
  EXPECT_EQ(GetParam().message,
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
  EXPECT_EQ(GetParam().has_value && 0 != GetParam().issue_count,
    omni::ryml::is_partial(result));
}

TEST_P(deserialization_diagnostics,
  transforms_diagnostics_and_preserves_optional_value) {
  const auto result = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(GetParam().tolerance)
      .allow_partial(GetParam().partial),
  }(omni::type_t<serialization_data::payload>{}, GetParam().source)
                        .map_diagnostics(omni::ryml::render_diangostics);

  EXPECT_EQ(GetParam().has_value, bool(result.value));
  EXPECT_EQ(GetParam().message, result.diagnostics);
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
      0,
      "",
      "ok",
      7},
    diagnostics_case{"first_error",
      0,
      false,
      R"({"name":{},"code":"bad"})",
      false,
      true,
      1,
      "/name: expected string, found object",
      "",
      0},
    diagnostics_case{"collect_without_value",
      2,
      false,
      R"({"name":{},"code":"bad"})",
      false,
      false,
      2,
      "/name: expected string, found object\n"
      "/code: \"bad\" is not an integer",
      "",
      0},
    diagnostics_case{"partial_first_error",
      0,
      true,
      R"({"name":"ok","code":"bad"})",
      true,
      true,
      1,
      "/code: \"bad\" is not an integer",
      "ok",
      0},
    diagnostics_case{"partial_budget_exhausted",
      1,
      true,
      R"({"name":{},"code":"bad"})",
      true,
      true,
      2,
      "/name: expected string, found object\n"
      "/code: \"bad\" is not an integer",
      "",
      0},
    diagnostics_case{"partial_budget_remaining",
      2,
      true,
      R"({"name":{},"code":"bad"})",
      true,
      false,
      2,
      "/name: expected string, found object\n"
      "/code: \"bad\" is not an integer",
      "",
      0},
    diagnostics_case{"unknown_field",
      1,
      true,
      R"({"name":"ok","code":7,"extra":1})",
      true,
      false,
      1,
      "/extra: unknown field",
      "ok",
      7},
    diagnostics_case{"duplicate_keeps_first",
      1,
      true,
      R"({"name":"ok","code":7,"code":8})",
      true,
      false,
      1,
      "/code: duplicate field",
      "ok",
      7},
    diagnostics_case{"missing_retains_default",
      1,
      true,
      R"({"name":"ok"})",
      true,
      false,
      1,
      "/code: missing field",
      "ok",
      0},
    diagnostics_case{"negative_budget_stops_first",
      -1,
      true,
      R"({"name":{},"code":"bad"})",
      true,
      true,
      1,
      "/name: expected string, found object",
      "",
      0},
    diagnostics_case{"wrong_root_shape",
      2,
      false,
      "[]",
      false,
      false,
      1,
      "<root>: expected object, found array",
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
  if (1U != result.diagnostics.issues.size())
    return;

  const auto &d = result.diagnostics.issues.front();
  EXPECT_EQ(omni::ryml::issue::code::invalid_scalar, d.reason);
  EXPECT_EQ(omni::ryml::issue::kind::integer, d.expected);
  EXPECT_EQ(omni::ryml::issue::kind::scalar, d.actual);
  EXPECT_EQ(2U, d.path.size());
  if (2U != d.path.size())
    return;
  EXPECT_EQ("data", omni::ryml::detail::to_string(d.path[0].field));
  EXPECT_EQ("code", omni::ryml::detail::to_string(d.path[1].field));
  EXPECT_EQ(omni::compat::nullopt, d.path[1].index);
  EXPECT_EQ(std::string::npos, d.offset);
  EXPECT_EQ(std::string::npos, d.line);
  EXPECT_EQ(std::string::npos, d.column);
  EXPECT_EQ("bad",
    omni::ryml::detail::to_string(result.diagnostics.tree.val(d.node_id)));
}

TEST(deserialization_diagnostics,
  nested_container_warnings_do_not_spend_budget) {
  const auto result = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(1).allow_partial(true),
  }(omni::type_t<serialization_data::scalar_values>{},
    R"({"enabled":true,"retries":2,"delta":3,"values":[1,"bad",3],
      "label":"ok","records":[{"name":"first","code":"bad"},
                              {"name":"unvisited","code":8}]})");

  EXPECT_TRUE(result.value);
  EXPECT_FALSE(result.diagnostics.stopped());
  EXPECT_EQ(2U, result.diagnostics.issues.size());
  EXPECT_EQ(
    "/values/1: warning: \"bad\" is not an integer\n"
    "/records/0/code: warning: \"bad\" is not an integer",
    omni::ryml::render_diangostics(result.diagnostics));
  if (2U != result.diagnostics.issues.size())
    return;

  const auto &path = result.diagnostics.issues.front().path;
  EXPECT_EQ(2U, path.size());
  if (2U != path.size())
    return;

  const auto &index = path[1];
  EXPECT_EQ((omni::compat::optional<std::size_t>{1U}), index.index);
  if (result.value) {
    EXPECT_EQ((std::vector<int>{1, 0, 3}), result.value->values);
    EXPECT_EQ(2U, result.value->records.size());
    if (2U != result.value->records.size())
      return;

    EXPECT_EQ("first", result.value->records[0].name);
    EXPECT_EQ(0, result.value->records[0].code);
    EXPECT_EQ("unvisited", result.value->records[1].name);
    EXPECT_EQ(8, result.value->records[1].code);
  }
}

TEST(deserialization_diagnostics, retained_diagnostics_own_views_after_moves) {
  omni::ryml::diagnostics</*owning=*/true> retained;
  {
    auto result =
      omni::ryml::deserialize(omni::type_t<serialization_data::payload>{},
        R"({"name":"ok","code":1,"some/unknown~field":2})");

    auto moved = std::move(result);
    retained = std::move(moved.diagnostics);
  }

  EXPECT_EQ("/some~1unknown~0field: unknown field",
    omni::ryml::render_diangostics(retained));
  EXPECT_EQ(1U, retained.issues.size());
  if (1U != retained.issues.size())
    return;

  EXPECT_EQ(1U, retained.issues.front().path.size());
  if (1U != retained.issues.front().path.size())
    return;

  EXPECT_EQ("some/unknown~field",
    omni::ryml::detail::to_string(retained.issues.front().path.front().field));
}

TEST(deserialization_diagnostics, moved_result_retains_value_and_scalar_views) {
  omni::ryml::with_diagnostics<serialization_data::payload> retained;
  {
    auto result = omni::ryml::deserialize_t{
      /*strategy=*/omni::ryml::use_tolerance(2).allow_partial(true),
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
    /*strategy=*/omni::ryml::use_tolerance(12).allow_partial(true),
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
  // Label used in the individual test name.
  std::string name;
  // Document containing the diagnostic path to render.
  std::string source;
  // Expected default diagnostic text.
  std::string message;
  // A syntax error prevents mapping and produces a parse_error issue.
  bool parse_failed;
};

class diagnostic_rendering: public testing::TestWithParam<rendering_case> {};

TEST_P(diagnostic_rendering, renders_paths_and_messages_on_error) {
  auto result = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(4),
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

  auto rendered = std::move(result).map_diagnostics([](std::unique_ptr<int> d) {
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
  // Label used in the individual test name.
  std::string name;
  // Malformed document supplied to the parser.
  std::string source;
};

class parser_recovery: public testing::TestWithParam<parser_failure_case> {};

TEST_P(parser_recovery, retains_failure_and_allows_the_next_invocation) {
  const auto deserialize = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(12).allow_partial(true),
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
    auto result =
      omni::ryml::deserialize(omni::type_t<serialization_data::payload>{},
        GetParam().source);
    auto moved = std::move(result);
    retained = std::move(moved).map_diagnostics(omni::ryml::render_diangostics);
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
        /*strategy=*/omni::ryml::use_tolerance(tolerance).allow_partial(
          partial),
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
        return std::string{0 > tolerance ? "negative"
                   : 0 == tolerance      ? "zero"
                                         : "positive"}
        + (partial ? "_partial" : "_strict");
      },
      p.param);
  });

struct initialized_payload {
  // Initialized fallback for a missing or invalid name.
  std::string name = "default";
  // Initialized fallback for a missing or invalid code.
  int code = 42;
};

TEST(deserialization_diagnostics,
  failed_and_missing_fields_keep_initialized_defaults) {
  const auto result = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(2).allow_partial(true),
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
    /*strategy=*/omni::ryml::use_tolerance(12).allow_partial(true),
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
    .strategy =
      omni::ryml::strategy{
        .tolerance = 2,
        .partial = true,
        .extra = false,
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
                        /*strategy=*/omni::ryml::use_tolerance(0),
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
  EXPECT_EQ(GetParam().issue_count, result.diagnostics.issues.size());
  EXPECT_EQ(GetParam().message,
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
    omni::ryml::diagnostics</*owning=*/false>>
    result = map(tree.crootref());

  EXPECT_EQ(GetParam().has_value, bool(result.value));
  EXPECT_EQ(GetParam().stopped, result.diagnostics.stopped());
  EXPECT_EQ(GetParam().partial, result.diagnostics.policy.partial);
  EXPECT_EQ(0 >= GetParam().tolerance
      ? 0U
      : static_cast<std::size_t>(GetParam().tolerance),
    result.diagnostics.policy.tolerance);
  EXPECT_EQ(GetParam().issue_count, result.diagnostics.issues.size());
  EXPECT_EQ(&tree, result.diagnostics.tree.tree());
  EXPECT_EQ(GetParam().has_value && 0 != GetParam().issue_count,
    omni::ryml::is_partial(result));
  EXPECT_EQ(GetParam().message,
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
    omni::ryml::diagnostics</*owning=*/false>>
    mapped = omni::ryml::map_tree(omni::type_t<serialization_data::payload>{},
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
    retained =
      omni::ryml::map_tree(omni::type_t<serialization_data::payload>{}, tree);

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
  EXPECT_EQ("7", omni::ryml::detail::to_string(tree.crootref()["code"].val()));
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
    EXPECT_EQ(subtree["code"].id(), result.diagnostics.issues.front().node_id);
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

TEST(tree_mapping, borrowed_container_warnings_do_not_spend_budget) {
  const auto tree = ::ryml::parse_in_arena(
    R"({"enabled":true,"retries":2,"delta":3,"values":[1,"bad",3],
      "label":"ok","records":[{"name":"first","code":"bad"},
                              {"name":"unvisited","code":8}]})");
  const auto result = omni::ryml::map_tree_t{
    /*strategy=*/omni::ryml::use_tolerance(1).allow_partial(true),
  }(omni::type_t<serialization_data::scalar_values>{}, tree.crootref());

  EXPECT_TRUE(result.value);
  EXPECT_FALSE(result.diagnostics.stopped());
  EXPECT_EQ(2U, result.diagnostics.issues.size());
  EXPECT_EQ(
    "/values/1: warning: \"bad\" is not an integer\n"
    "/records/0/code: warning: \"bad\" is not an integer",
    omni::ryml::render_diangostics(result.diagnostics));
  if (result.value) {
    EXPECT_EQ((std::vector<int>{1, 0, 3}), result.value->values);
    EXPECT_EQ(2U, result.value->records.size());
    if (2U == result.value->records.size()) {
      EXPECT_EQ("first", result.value->records[0].name);
      EXPECT_EQ(0, result.value->records[0].code);
      EXPECT_EQ("unvisited", result.value->records[1].name);
      EXPECT_EQ(8, result.value->records[1].code);
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

TEST(tree_mapping, empty_model_accepts_extra_fields) {
  const auto result = omni::ryml::map_tree_t{
    /*strategy=*/omni::ryml::use_tolerance(0).allow_extra(true),
  }(omni::type_t<empty_record>{},
    ::ryml::parse_in_arena(R"({"extra":1,"extra":2,"nested":{"more":[]}})"));

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.policy.extra);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  EXPECT_FALSE(result.diagnostics.stopped());
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

#if defined(__cpp_designated_initializers) \
  && 201707L <= __cpp_designated_initializers
TEST(tree_mapping, supports_designated_configuration) {
  constexpr omni::ryml::map_tree_t configured{
    .strategy =
      omni::ryml::strategy{
        .tolerance = 2,
        .partial = true,
        .extra = false,
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

struct optional_config {
  // Initialized fallback for missing or invalid supplied values.
  omni::compat::optional<int> limit = 7;
  // Shows whether traversal continued beyond the optional field.
  int following = 42;
};

// Optional input and its retained value, crossed with strict/partial policies.
struct optional_case {
  // Label used in the individual test name.
  std::string name;
  // Document containing the optional field and a following scalar.
  std::string source;
  // Optional value expected after successful or partial deserialization.
  omni::compat::optional<int> expected;
  // Whether the supplied optional value needs recovery.
  bool invalid;
};

class optional_recovery:
    public testing::TestWithParam<std::tuple<optional_case, bool>> {};

TEST_P(optional_recovery, applies_the_partial_warning_policy) {
  omni::compat::apply(
    [](const optional_case &c, bool partial) {
      const auto result = omni::ryml::deserialize_t{
        /*strategy=*/omni::ryml::use_tolerance(0).allow_partial(partial),
      }(omni::type_t<optional_config>{}, c.source);

      EXPECT_EQ(!c.invalid || partial, bool(result.value));
      EXPECT_EQ(c.invalid && !partial, result.diagnostics.stopped());
      EXPECT_EQ(c.invalid ? 1U : 0U, result.diagnostics.issues.size());
      if (result.value) {
        EXPECT_EQ(c.expected, result.value->limit);
        EXPECT_EQ(7, result.value->following);
      }
      if (1U == result.diagnostics.issues.size()) {
        EXPECT_EQ(partial, result.diagnostics.issues.front().warning);
        EXPECT_EQ(1U, result.diagnostics.issues.front().path.size());
        if (1U == result.diagnostics.issues.front().path.size()) {
          EXPECT_EQ("limit",
            omni::ryml::detail::to_string(
              result.diagnostics.issues.front().path.front().field));
        }
      }
    },
    GetParam());
}

INSTANTIATE_TEST_SUITE_P(fields,
  optional_recovery,
  testing::Combine(
    testing::Values( //
      optional_case{"missing", R"({"following":7})", 7, false},
      optional_case{"null",
        R"({"limit":null,"following":7})",
        omni::compat::nullopt,
        false},
      optional_case{"valid", R"({"limit":9,"following":7})", 9, false},
      optional_case{"minimum",
        "{\"limit\":" + std::to_string(std::numeric_limits<int>::min())
          + ",\"following\":7}",
        std::numeric_limits<int>::min(),
        false},
      optional_case{"maximum",
        "{\"limit\":" + std::to_string(std::numeric_limits<int>::max())
          + ",\"following\":7}",
        std::numeric_limits<int>::max(),
        false},
      optional_case{"invalid_scalar",
        R"({"limit":"bad","following":7})",
        7,
        true},
      optional_case{"wrong_kind", R"({"limit":[],"following":7})", 7, true},
      optional_case{"duplicate",
        R"({"limit":9,"limit":10,"following":7})",
        9,
        true},
      optional_case{"out_of_range",
        R"({"limit":999999999999999999999999999999,"following":7})",
        7,
        true},
      optional_case{"negative_out_of_range",
        R"({"limit":-999999999999999999999999999999,"following":7})",
        7,
        true}),
    testing::Bool()),
  [](const testing::TestParamInfo<std::tuple<optional_case, bool>> &p) {
    return omni::compat::apply(
      [](const optional_case &c, bool partial) {
        return c.name + (partial ? "_partial" : "_strict");
      },
      p.param);
  });

// Container failures and their diagnostic text, crossed with retention policy.
struct container_failure_case {
  // Label used in the individual test name.
  std::string name;
  // Missing or wrong-kind container followed by a valid scalar.
  std::string source;
  // Message following the path and optional warning label.
  std::string message;
};

class container_recovery:
    public testing::TestWithParam<std::tuple<container_failure_case, bool>> {};

TEST_P(container_recovery, retains_defaults_and_classifies_the_issue) {
  omni::compat::apply(
    [](const container_failure_case &c, bool partial) {
      const auto result = omni::ryml::deserialize_t{
        /*strategy=*/omni::ryml::use_tolerance(0).allow_partial(partial),
      }(omni::type_t<container_config>{}, c.source);

      EXPECT_EQ(partial, bool(result.value));
      EXPECT_EQ(!partial, result.diagnostics.stopped());
      EXPECT_EQ(1U, result.diagnostics.issues.size());
      if (1U == result.diagnostics.issues.size()) {
        EXPECT_EQ(partial, result.diagnostics.issues.front().warning);
      }
      EXPECT_EQ(std::string{"/values: "} + (partial ? "warning: " : "")
          + c.message,
        omni::ryml::render_diangostics(result.diagnostics));
      if (result.value) {
        EXPECT_EQ((std::vector<int>{9}), result.value->values);
        EXPECT_EQ(7, result.value->following);
      }
    },
    GetParam());
}

INSTANTIATE_TEST_SUITE_P(fields,
  container_recovery,
  testing::Combine(
    testing::Values( //
      container_failure_case{"missing", R"({"following":7})", "missing field"},
      container_failure_case{"wrong_kind",
        R"({"values":{},"following":7})",
        "expected array, found object"}),
    testing::Bool()),
  [](const testing::TestParamInfo< //
    std::tuple<container_failure_case, bool>> &p) {
    return omni::compat::apply(
      [](const container_failure_case &c, bool partial) {
        return c.name + (partial ? "_partial" : "_strict");
      },
      p.param);
  });

struct optional_record_config {
  // Invalid supplied records leave this optional empty.
  omni::compat::optional<initialized_payload> field;
  // Shows whether nested warnings allowed traversal to continue.
  int following = 42;
};

TEST(deserialization_warnings, duplicate_containers_warn_and_keep_first_value) {
  const auto result = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(0).allow_partial(true),
  }(omni::type_t<container_config>{},
    R"({"values":[1],"values":[2],"following":7})");

  EXPECT_TRUE(result.value);
  EXPECT_FALSE(result.diagnostics.stopped());
  EXPECT_EQ("/values: warning: duplicate field",
    omni::ryml::render_diangostics(result.diagnostics));
  if (result.value) {
    EXPECT_EQ((std::vector<int>{1}), result.value->values);
    EXPECT_EQ(7, result.value->following);
  }
}

TEST(deserialization_warnings, optional_records_inherit_the_warning_policy) {
  const auto result = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(0).allow_partial(true),
  }(omni::type_t<optional_record_config>{},
    R"({"field":{"name":"ok","code":"bad"},"following":7})");

  EXPECT_TRUE(result.value);
  EXPECT_FALSE(result.diagnostics.stopped());
  EXPECT_EQ("/field/code: warning: \"bad\" is not an integer",
    omni::ryml::render_diangostics(result.diagnostics));
  if (result.value) {
    EXPECT_FALSE(result.value->field);
    EXPECT_EQ(7, result.value->following);
  }
}

struct optional_sequence_config {
  // Optional entries preserve null and failed entries at their source indices.
  std::vector<omni::compat::optional<int>> values;
};

TEST(deserialization_warnings, containers_accept_optional_elements) {
  const auto result = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(0).allow_partial(true),
  }(omni::type_t<optional_sequence_config>{}, R"({"values":[1,null,"bad",3]})");

  EXPECT_TRUE(result.value);
  EXPECT_FALSE(result.diagnostics.stopped());
  EXPECT_EQ("/values/2: warning: \"bad\" is not an integer",
    omni::ryml::render_diangostics(result.diagnostics));
  if (result.value) {
    EXPECT_EQ((std::vector<omni::compat::optional<int>>{1,
                omni::compat::nullopt,
                omni::compat::nullopt,
                3}),
      result.value->values);
  }
}

struct mixed_config {
  // Warnings here must not spend the following scalar's error budget.
  std::vector<int> values;
  // Required scalar with an initialized fallback.
  int required = 11;
  // Shows where the first hard error stopped traversal.
  int following = 42;
};

TEST(deserialization_warnings, scalar_errors_stop_after_container_warnings) {
  const auto result = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(0).allow_partial(true),
  }(omni::type_t<mixed_config>{},
    R"({"values":["bad",3],"required":"bad","following":7})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.stopped());
  EXPECT_EQ(2U, result.diagnostics.issues.size());
  if (2U == result.diagnostics.issues.size()) {
    EXPECT_TRUE(result.diagnostics.issues.front().warning);
    EXPECT_FALSE(result.diagnostics.issues.back().warning);
  }
  EXPECT_EQ(
    "/values/0: warning: \"bad\" is not an integer\n"
    "/required: \"bad\" is not an integer",
    omni::ryml::render_diangostics(result.diagnostics));
  if (result.value) {
    EXPECT_EQ((std::vector<int>{0, 3}), result.value->values);
    EXPECT_EQ(11, result.value->required);
    EXPECT_EQ(42, result.value->following);
  }
}

TEST(deserialization_warnings, syntax_errors_remain_errors_in_partial_mode) {
  const auto result = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(0).allow_partial(true),
  }(omni::type_t<optional_config>{}, R"({"limit":)");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (1U == result.diagnostics.issues.size()) {
    EXPECT_FALSE(result.diagnostics.issues.front().warning);
    EXPECT_EQ(omni::ryml::issue::code::parse_error,
      result.diagnostics.issues.front().reason);
  }
}

struct required_record_config {
  // First required scalar, before the nested record.
  int first = 11;
  // Required record whose scalar errors share the root's tolerance.
  initialized_payload nested;
  // Shows where the second hard error stopped traversal.
  int following = 42;
};

TEST(deserialization_warnings, required_records_share_the_hard_error_budget) {
  const auto result = omni::ryml::deserialize_t{
    /*strategy=*/omni::ryml::use_tolerance(1).allow_partial(true),
  }(omni::type_t<required_record_config>{},
    R"({"first":"bad","nested":{"name":"ok","code":"bad"},"following":7})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.stopped());
  EXPECT_EQ(2U, result.diagnostics.issues.size());
  for (const auto &e : result.diagnostics.issues) {
    SCOPED_TRACE(e.node_id);
    EXPECT_FALSE(e.warning);
  }
  EXPECT_EQ(
    "/first: \"bad\" is not an integer\n"
    "/nested/code: \"bad\" is not an integer",
    omni::ryml::render_diangostics(result.diagnostics));
  if (result.value) {
    EXPECT_EQ(11, result.value->first);
    EXPECT_EQ("ok", result.value->nested.name);
    EXPECT_EQ(42, result.value->nested.code);
    EXPECT_EQ(42, result.value->following);
  }
}

constexpr omni::ryml::serialize_t json_serializer{
  /*format=*/::ryml::EMIT_JSON,
};
static_assert(::ryml::EMIT_YAML == omni::ryml::as_yaml.format,
  "as_yaml must emit YAML");
static_assert(::ryml::EMIT_JSON == omni::ryml::as_json.format,
  "as_json must emit JSON");
static_assert(::ryml::EMIT_JSON == json_serializer.format,
  "serializer configuration must support C++11 constant expressions");

#if defined(__cpp_designated_initializers) \
  && 201707L <= __cpp_designated_initializers
constexpr omni::ryml::serialize_t designated_serializer{
  .format = ::ryml::EMIT_JSON,
};
static_assert(std::is_aggregate<omni::ryml::serialize_t>::value,
  "serializers must support aggregate configuration");
#endif

struct empty_values {
  // Empty text must remain a quoted scalar.
  std::string name;
  // An empty scalar sequence.
  std::vector<int> values;
  // An empty record sequence.
  std::vector<serialization_data::payload> records;
  // An empty nested record.
  empty_record object;
};

TEST_P(serialization, retains_empty_strings_containers_and_records) {
  const auto output = omni::ryml::serialize_t{
    /*format=*/GetParam(),
  }(empty_values{});
  const auto parsed = omni::ryml::parse(output);

  EXPECT_TRUE(parsed);
  if (!parsed)
    return;

  const auto root = parsed->crootref();
  EXPECT_TRUE(root.is_map());
  if (!root.is_map())
    return;

  for (const auto name :
    std::initializer_list<c4::csubstr>{"name", "values", "records", "object"}) {
    SCOPED_TRACE(omni::ryml::detail::to_string(name));
    EXPECT_TRUE(root.has_child(name));
    if (!root.has_child(name))
      return;
  }

  EXPECT_TRUE(root["name"].val().empty());
  EXPECT_TRUE(root["name"].is_val_quoted());
  EXPECT_TRUE(root["values"].is_seq());
  EXPECT_EQ(0U, root["values"].num_children());
  EXPECT_TRUE(root["records"].is_seq());
  EXPECT_EQ(0U, root["records"].num_children());
  EXPECT_TRUE(root["object"].is_map());
  EXPECT_EQ(0U, root["object"].num_children());

  const auto restored =
    omni::ryml::deserialize(omni::type_t<empty_values>{}, output);
  EXPECT_TRUE(restored.value);
  EXPECT_TRUE(restored.diagnostics.issues.empty());
}

TEST_P(serialization, writes_an_empty_root_record) {
  const auto parsed = omni::ryml::parse(omni::ryml::serialize_t{
    /*format=*/GetParam(),
  }(empty_record{}));

  EXPECT_TRUE(parsed);
  if (!parsed)
    return;

  EXPECT_TRUE(parsed->crootref().is_map());
  EXPECT_EQ(0U, parsed->crootref().num_children());
}

struct boolean_values {
  // Scalar boolean to emit as a word.
  bool enabled;
  // Packed boolean sequence to emit as words.
  std::vector<bool> flags;
};

TEST_P(serialization, writes_booleans_and_boolean_sequences_as_words) {
  const auto parsed = omni::ryml::parse(omni::ryml::serialize_t{
    /*format=*/GetParam(),
  }(boolean_values{false, {true, false}}));

  EXPECT_TRUE(parsed);
  if (!parsed)
    return;

  const auto root = parsed->crootref();
  EXPECT_TRUE(root.is_map());
  if (!root.is_map())
    return;

  EXPECT_TRUE(root.has_child("enabled"));
  EXPECT_TRUE(root.has_child("flags"));
  if (!root.has_child("enabled") || !root.has_child("flags"))
    return;

  EXPECT_TRUE(root["flags"].is_seq());
  EXPECT_EQ(2U, root["flags"].num_children());
  if (!root["flags"].is_seq() || 2U != root["flags"].num_children())
    return;

  EXPECT_EQ("false", root["enabled"].val());
  EXPECT_FALSE(root["enabled"].is_val_quoted());
  EXPECT_EQ("true", root["flags"][0].val());
  EXPECT_FALSE(root["flags"][0].is_val_quoted());
  EXPECT_EQ("false", root["flags"][1].val());
  EXPECT_FALSE(root["flags"][1].is_val_quoted());
}

TEST_P(serialization, reads_bitfields_without_mutating_the_source) {
  const serialization_data::bitfield_values value{815, true};
  const auto parsed = omni::ryml::parse(omni::ryml::serialize_t{
    /*format=*/GetParam(),
  }(value));

  EXPECT_TRUE(parsed);
  if (!parsed)
    return;

  const auto root = parsed->crootref();
  EXPECT_TRUE(root.is_map());
  if (!root.is_map())
    return;

  EXPECT_TRUE(root.has_child("code"));
  EXPECT_TRUE(root.has_child("enabled"));
  if (!root.has_child("code") || !root.has_child("enabled"))
    return;

  EXPECT_EQ("815", root["code"].val());
  EXPECT_EQ("true", root["enabled"].val());
  EXPECT_EQ(815U, value.code);
  EXPECT_TRUE(value.enabled);
}

struct read_only_record {
  // Const public field included in serialization.
  const int code = 42;
  // Mutable public field included in serialization.
  std::string name = "Ada";

  private:
  // Private field omitted from serialization.
  int hidden = 815;
};

TEST_P(serialization, includes_const_public_fields_and_omits_private_fields) {
  const auto parsed = omni::ryml::parse(omni::ryml::serialize_t{
    /*format=*/GetParam(),
  }(read_only_record{}));

  EXPECT_TRUE(parsed);
  if (!parsed)
    return;

  const auto root = parsed->crootref();
  EXPECT_TRUE(root.is_map());
  if (!root.is_map())
    return;

  EXPECT_EQ(2U, root.num_children());
  EXPECT_TRUE(root.has_child("code"));
  EXPECT_TRUE(root.has_child("name"));
  if (!root.has_child("code") || !root.has_child("name"))
    return;

  EXPECT_EQ("42", root["code"].val());
  EXPECT_EQ("Ada", root["name"].val());
  EXPECT_FALSE(root.has_child("hidden"));
}

struct numeric_values {
  // First representable float above one.
  float ratio;
  // First representable double above one.
  double precise;
  // Smallest signed 64-bit integer.
  std::int64_t minimum;
  // Largest unsigned 64-bit integer.
  std::uint64_t maximum;
  // Floating-point values at the limits and below zero.
  std::vector<double> values;
};

TEST_P(serialization,
  preserves_integer_boundaries_and_floating_point_precision) {
  const numeric_values value{
    std::nextafter(1.0f, 2.0f),
    std::nextafter(1.0, 2.0),
    std::numeric_limits<std::int64_t>::min(),
    std::numeric_limits<std::uint64_t>::max(),
    {std::numeric_limits<double>::min(),
      std::numeric_limits<double>::max(),
      -0.125},
  };
  const auto output = omni::ryml::serialize_t{
    /*format=*/GetParam(),
  }(value);
  const auto restored =
    omni::ryml::deserialize(omni::type_t<numeric_values>{}, output);

  EXPECT_TRUE(restored.value);
  EXPECT_TRUE(restored.diagnostics.issues.empty());
  if (!restored.value)
    return;

  EXPECT_EQ(value.ratio, restored.value->ratio);
  EXPECT_EQ(value.precise, restored.value->precise);
  EXPECT_EQ(value.minimum, restored.value->minimum);
  EXPECT_EQ(value.maximum, restored.value->maximum);
  EXPECT_EQ(value.values, restored.value->values);
}

TEST_P(serialization, serialized_text_outlives_the_source) {
  std::string output;
  {
    serialization_data::payload value{std::string(4096, 'x'), 815};
    output = omni::ryml::serialize_t{
      /*format=*/GetParam(),
    }(value);
    value.name.assign(4096, 'y');
  }
  const auto restored =
    omni::ryml::deserialize(omni::type_t<serialization_data::payload>{},
      output);

  EXPECT_TRUE(restored.value);
  EXPECT_TRUE(restored.diagnostics.issues.empty());
  if (restored.value) {
    EXPECT_EQ(std::string(4096, 'x'), restored.value->name);
    EXPECT_EQ(815, restored.value->code);
  }
}

INSTANTIATE_TEST_SUITE_P(formats,
  serialization,
  testing::Values(::ryml::EMIT_YAML, ::ryml::EMIT_JSON),
  [](const testing::TestParamInfo<::ryml::EmitType_e> &p) {
    return ::ryml::EMIT_JSON == p.param ? "json" : "yaml";
  });

// Text that could be mistaken for another document type or needs escaping.
struct string_case {
  // Label used in the individual test name.
  std::string name;
  // String value that must survive serialization and parsing unchanged.
  std::string text;
};

class serialized_strings:
    public testing::TestWithParam<std::tuple<::ryml::EmitType_e, string_case>> {
};

TEST_P(serialized_strings,
  preserves_string_types_and_escapes_special_characters) {
  omni::compat::apply(
    [](::ryml::EmitType_e format, const string_case &c) {
      const auto output = omni::ryml::serialize_t{
        /*format=*/format,
      }(serialization_data::payload{c.text, 815});
      const auto parsed = omni::ryml::parse(output);

      EXPECT_TRUE(parsed);
      if (!parsed)
        return;

      const auto root = parsed->crootref();
      EXPECT_TRUE(root.is_map());
      if (!root.is_map())
        return;

      EXPECT_TRUE(root.has_child("name"));
      if (!root.has_child("name"))
        return;

      const auto name = root["name"];
      EXPECT_TRUE(name.is_val_quoted());
      EXPECT_EQ(c4::to_csubstr(c.text), name.val());
    },
    GetParam());
}

INSTANTIATE_TEST_SUITE_P(values,
  serialized_strings,
  testing::Combine(testing::Values(::ryml::EMIT_YAML, ::ryml::EMIT_JSON),
    testing::Values( //
      string_case{"empty", ""},
      string_case{"unicode", "oceanic \xCE\xA9"},
      string_case{"boolean_true", "true"},
      string_case{"boolean_false", "false"},
      string_case{"null_word", "null"},
      string_case{"integer", "12"},
      string_case{"leading_zeroes", "001"},
      string_case{"exponent", "1e2"},
      string_case{"comment", "# comment"},
      string_case{"leading_space", " leading space"},
      string_case{"escapes", "quote\" slash\\ newline\n tab\t"},
      string_case{"embedded_null", std::string{"a\0b", 3}})),
  [](const testing::TestParamInfo< //
    std::tuple<::ryml::EmitType_e, string_case>> &p) {
    return omni::compat::apply(
      [](::ryml::EmitType_e format, const string_case &c) {
        return std::string{::ryml::EMIT_JSON == format ? "json_" : "yaml_"}
        + c.name;
      },
      p.param);
  });

#if defined(__cpp_designated_initializers) \
  && 201707L <= __cpp_designated_initializers
TEST(serialization_formats, supports_designated_configuration) {
  const auto output =
    designated_serializer(serialization_data::payload{"Ada", 815});
  EXPECT_FALSE(output.empty());
  if (output.empty())
    return;

  EXPECT_EQ('{', output.front());
}
#endif

TEST(serialization_json, escapes_control_bytes_as_unicode) {
  const auto output = omni::ryml::as_json(serialization_data::payload{
    std::string{"a\0b\x01"
                "c\x1f",
      6},
    815,
  });

  EXPECT_NE(std::string::npos, output.find(R"("a\u0000b\u0001c\u001f")"));
}

} // namespace
