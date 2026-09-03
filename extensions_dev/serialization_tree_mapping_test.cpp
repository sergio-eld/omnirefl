#include "serialization/data.hpp"
#include "serialization/map_tree.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

TEST(tree_mapping, maps_a_representative_document) {
  const auto result = omni::ryml::deserialize(omni::ryml::strategy{},
    omni::type_t<serialization_data::document>{},
    std::string{serialization_data::representative_json});

  ASSERT_TRUE(result) << result.error();
  EXPECT_EQ(3U, result->schema_version);
  EXPECT_EQ("checkout-api", result->source);
  ASSERT_EQ(3U, result->orders.size());
  EXPECT_EQ(8150001U, result->orders[0].id);
  EXPECT_EQ("oceanic-labs", result->orders[0].customer);
  EXPECT_TRUE(result->orders[0].expedited);
  EXPECT_EQ("Lisbon", result->orders[0].shipping.city);
  EXPECT_EQ("Rua do Oceano 815", result->orders[0].shipping.street);
  EXPECT_DOUBLE_EQ(38.7223, result->orders[0].shipping.location.latitude);
  EXPECT_DOUBLE_EQ(-9.1393, result->orders[0].shipping.location.longitude);
  ASSERT_EQ(3U, result->orders[0].items.size());
  EXPECT_EQ("sensor-815", result->orders[0].items[0].sku);
  EXPECT_EQ("Ocean sensor", result->orders[0].items[0].description);
  EXPECT_EQ(4U, result->orders[0].items[0].quantity);
  EXPECT_DOUBLE_EQ(42.5, result->orders[0].items[0].unit_price);
  EXPECT_TRUE(result->orders[0].items[0].taxable);
  EXPECT_EQ((std::vector<int>{8, 15, 42, 108}), result->orders[0].checkpoints);
  EXPECT_EQ(8150003U, result->orders[2].id);
  EXPECT_EQ("pelagic-systems", result->orders[2].customer);
}

TEST(tree_mapping, maps_the_same_schema_from_yaml) {
  const auto result = omni::ryml::deserialize(omni::ryml::strategy{},
    omni::type_t<serialization_data::document>{},
    std::string{serialization_data::representative_yaml});

  ASSERT_TRUE(result) << result.error();
  EXPECT_EQ(3U, result->schema_version);
  EXPECT_EQ("checkout-api", result->source);
  ASSERT_EQ(3U, result->orders.size());
  EXPECT_EQ("northwind-research", result->orders[1].customer);
  EXPECT_EQ("Reykjavik", result->orders[1].shipping.city);
  ASSERT_EQ(3U, result->orders[1].items.size());
  EXPECT_EQ("probe-42", result->orders[1].items[0].sku);
  EXPECT_DOUBLE_EQ(815.0, result->orders[1].items[0].unit_price);
}

TEST(tree_mapping, maps_scalars_sequences_and_nested_records) {
  const auto result = omni::ryml::deserialize(omni::ryml::strategy{},
    omni::type_t<serialization_data::scalar_values>{},
    std::string{serialization_data::scalar_json});

  ASSERT_TRUE(result) << result.error();
  EXPECT_TRUE(result->enabled);
  EXPECT_EQ(108U, result->retries);
  EXPECT_EQ(-42, result->delta);
  EXPECT_EQ((std::vector<int>{8, 15}), result->values);
  EXPECT_EQ("oceanic", result->label);
  ASSERT_EQ(2U, result->records.size());
  EXPECT_EQ("oceanic", result->records[0].name);
  EXPECT_EQ(815, result->records[0].code);
  EXPECT_EQ("sunset", result->records[1].name);
  EXPECT_EQ(108, result->records[1].code);
}

TEST(tree_mapping, assigns_bitfields_through_field_bindings) {
  const auto result = omni::ryml::deserialize(omni::ryml::strategy{},
    omni::type_t<serialization_data::bitfield_values>{},
    std::string{serialization_data::bitfield_json});

  ASSERT_TRUE(result) << result.error();
  EXPECT_EQ(815U, result->code);
  EXPECT_TRUE(result->enabled);
}

TEST(tree_mapping, reports_the_schema_scalar_expected_by_a_field) {
  const auto result = omni::ryml::deserialize(omni::ryml::strategy{},
    omni::type_t<serialization_data::nested_record>{},
    std::string{serialization_data::invalid_nested_integer_json});

  ASSERT_FALSE(result);
  EXPECT_EQ("\"invalid\" is not an integer", result.error());
}

TEST(tree_mapping, rejects_numeric_boolean_values) {
  const auto result = omni::ryml::deserialize(omni::ryml::strategy{},
    omni::type_t<serialization_data::bitfield_values>{},
    std::string{R"({"code":815,"enabled":108})"});

  ASSERT_FALSE(result);
  EXPECT_EQ("\"108\" is not a boolean", result.error());
}

TEST(tree_mapping, rejects_quoted_boolean_values) {
  const auto result = omni::ryml::deserialize(omni::ryml::strategy{},
    omni::type_t<serialization_data::bitfield_values>{},
    std::string{R"({"code":815,"enabled":"true"})"});

  ASSERT_FALSE(result);
  EXPECT_EQ("\"true\" is not a boolean", result.error());
}

TEST(tree_mapping, partially_applies_strategy_and_destination) {
  const auto deserialize = omni::fn::partial(omni::ryml::deserialize,
    omni::ryml::strategy{},
    omni::type_t<serialization_data::bitfield_values>{});
  const auto result =
    deserialize(std::string{serialization_data::bitfield_json});

  ASSERT_TRUE(result) << result.error();
  EXPECT_EQ(815U, result->code);
  EXPECT_TRUE(result->enabled);
}

TEST(tree_mapping, rejects_unknown_input_fields_by_default) {
  const auto result = omni::ryml::deserialize(omni::ryml::strategy{},
    omni::type_t<serialization_data::nested_record>{},
    std::string{serialization_data::unknown_field_json});

  ASSERT_FALSE(result);
  EXPECT_EQ("unknown field 'extra'", result.error());
}

TEST(tree_mapping, rejects_duplicate_input_fields_by_default) {
  const auto result = omni::ryml::deserialize(omni::ryml::strategy{},
    omni::type_t<serialization_data::nested_record>{},
    std::string{serialization_data::duplicate_field_json});

  ASSERT_FALSE(result);
  EXPECT_EQ("duplicate field 'ratio'", result.error());
}

TEST(tree_mapping, rejects_missing_destination_fields_by_default) {
  const auto result = omni::ryml::deserialize(omni::ryml::strategy{},
    omni::type_t<serialization_data::nested_record>{},
    std::string{serialization_data::missing_field_json});

  ASSERT_FALSE(result);
  EXPECT_EQ("missing field 'data'", result.error());
}

} // namespace
