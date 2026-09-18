#include "data.hpp"

#include <omnirefl/serialization/ryml.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <vector>

namespace {

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
  EXPECT_EQ(2U, root.num_children());
  EXPECT_EQ("Ada", root["name"].val());
  EXPECT_TRUE(root["name"].is_val_quoted());
  EXPECT_EQ("815", root["code"].val());
  EXPECT_FALSE(root["code"].is_val_quoted());
  if (::ryml::EMIT_JSON == GetParam())
    EXPECT_EQ('{', output.front());
  else
    EXPECT_EQ(0U, output.find("name:"));
}

TEST_P(serialization, writes_nested_records_and_sequences) {
  const serialization_data::scalar_values value{
    true, 108, -42, {8, 15}, "oceanic", {{"Ada", 815}, {"Grace", 108}},
  };
  const auto output = omni::ryml::serialize_t{
    /*format=*/GetParam(),
  }(value);
  const auto parsed = omni::ryml::parse(output);

  EXPECT_TRUE(parsed);
  if (!parsed)
    return;

  const auto root = parsed->crootref();
  EXPECT_EQ(6U, root.num_children());
  EXPECT_EQ("true", root["enabled"].val());
  EXPECT_FALSE(root["enabled"].is_val_quoted());
  EXPECT_EQ("108", root["retries"].val());
  EXPECT_EQ("-42", root["delta"].val());
  EXPECT_TRUE(root["values"].is_seq());
  EXPECT_EQ(2U, root["values"].num_children());
  EXPECT_EQ("8", root["values"][0].val());
  EXPECT_EQ("15", root["values"][1].val());
  EXPECT_EQ("oceanic", root["label"].val());
  EXPECT_TRUE(root["records"].is_seq());
  EXPECT_EQ(2U, root["records"].num_children());
  EXPECT_TRUE(root["records"][0].is_map());
  EXPECT_EQ("Ada", root["records"][0]["name"].val());
  EXPECT_EQ("815", root["records"][0]["code"].val());
  EXPECT_EQ("Grace", root["records"][1]["name"].val());
  EXPECT_EQ("108", root["records"][1]["code"].val());
}

struct empty_record {};

struct empty_values {
  std::string name;
  std::vector<int> values;
  std::vector<serialization_data::payload> records;
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
  EXPECT_TRUE(root["name"].val().empty());
  EXPECT_TRUE(root["name"].is_val_quoted());
  EXPECT_TRUE(root["values"].is_seq());
  EXPECT_EQ(0U, root["values"].num_children());
  EXPECT_TRUE(root["records"].is_seq());
  EXPECT_EQ(0U, root["records"].num_children());
  EXPECT_TRUE(root["object"].is_map());
  EXPECT_EQ(0U, root["object"].num_children());

  const auto restored = omni::ryml::deserialize(
    omni::type_t<empty_values>{}, output);
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
  bool enabled;
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

  EXPECT_EQ("815", parsed->crootref()["code"].val());
  EXPECT_EQ("true", parsed->crootref()["enabled"].val());
  EXPECT_EQ(815U, value.code);
  EXPECT_TRUE(value.enabled);
}

struct read_only_record {
  const int code = 42;
  std::string name = "Ada";

private:
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
  EXPECT_EQ(2U, root.num_children());
  EXPECT_EQ("42", root["code"].val());
  EXPECT_EQ("Ada", root["name"].val());
  EXPECT_FALSE(root.has_child("hidden"));
}

struct numeric_values {
  float ratio;
  double precise;
  std::int64_t minimum;
  std::uint64_t maximum;
  std::vector<double> values;
};

TEST_P(serialization, preserves_integer_boundaries_and_floating_point_precision) {
  const numeric_values value{
    std::nextafter(1.0f, 2.0f),
    std::nextafter(1.0, 2.0),
    std::numeric_limits<std::int64_t>::min(),
    std::numeric_limits<std::uint64_t>::max(),
    {std::numeric_limits<double>::min(),
      std::numeric_limits<double>::max(), -0.125},
  };
  const auto output = omni::ryml::serialize_t{
    /*format=*/GetParam(),
  }(value);
  const auto restored = omni::ryml::deserialize(
    omni::type_t<numeric_values>{}, output);

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
  const auto restored = omni::ryml::deserialize(
    omni::type_t<serialization_data::payload>{}, output);

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

class serialized_strings:
    public testing::TestWithParam<std::tuple<::ryml::EmitType_e, std::string>> {};

TEST_P(serialized_strings, preserves_string_types_and_escapes_special_characters) {
  omni::compat::apply(
    [](::ryml::EmitType_e format, const std::string &text) {
      const auto output = omni::ryml::serialize_t{
        /*format=*/format,
      }(serialization_data::payload{text, 815});
      const auto parsed = omni::ryml::parse(output);

      EXPECT_TRUE(parsed);
      if (!parsed)
        return;

      const auto name = parsed->crootref()["name"];
      EXPECT_TRUE(name.is_val_quoted());
      EXPECT_EQ(c4::to_csubstr(text), name.val());
    },
    GetParam());
}

INSTANTIATE_TEST_SUITE_P(values,
  serialized_strings,
  testing::Combine(testing::Values(::ryml::EMIT_YAML, ::ryml::EMIT_JSON),
    testing::Values(std::string{}, std::string{"true"}, std::string{"false"},
      std::string{"null"}, std::string{"12"}, std::string{"001"},
      std::string{"1e2"}, std::string{"# comment"}, std::string{" leading space"},
      std::string{"quote\" slash\\ newline\n tab\t"},
      std::string{"oceanic \xCE\xA9"}, std::string{"a\0b", 3})));

TEST(serialization_formats, emits_yaml_without_configuration) {
  const auto output = omni::ryml::as_yaml(
    serialization_data::payload{"Ada", 815});
  EXPECT_EQ(0U, output.find("name:"));

  const auto restored = omni::ryml::deserialize(
    omni::type_t<serialization_data::payload>{}, output);
  EXPECT_TRUE(restored.value);
  EXPECT_TRUE(restored.diagnostics.issues.empty());
  if (restored.value) {
    EXPECT_EQ("Ada", restored.value->name);
    EXPECT_EQ(815, restored.value->code);
  }
}

#if defined(__cpp_designated_initializers) \
  && 201707L <= __cpp_designated_initializers
TEST(serialization_formats, supports_designated_configuration) {
  const auto output =
    designated_serializer(serialization_data::payload{"Ada", 815});
  EXPECT_EQ('{', output.front());
}
#endif

TEST(serialization_formats, emits_json_without_configuration) {
  const auto output = omni::ryml::as_json(
    serialization_data::payload{"Ada", 815});
  EXPECT_EQ('{', output.front());

  const auto restored = omni::ryml::deserialize(
    omni::type_t<serialization_data::payload>{}, output);
  EXPECT_TRUE(restored.value);
  EXPECT_TRUE(restored.diagnostics.issues.empty());
  if (restored.value) {
    EXPECT_EQ("Ada", restored.value->name);
    EXPECT_EQ(815, restored.value->code);
  }
}

TEST(serialization_json, escapes_control_bytes_as_unicode) {
  const auto output = omni::ryml::as_json(serialization_data::payload{
    std::string{"a\0b\x01" "c\x1f", 6}, 815,
  });

  EXPECT_NE(std::string::npos,
    output.find(R"("a\u0000b\u0001c\u001f")"));
}

} // namespace
