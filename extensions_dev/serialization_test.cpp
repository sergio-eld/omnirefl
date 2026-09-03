#include "serialization/data.hpp"
#include "serialization/deserialize_composed.hpp"

#if defined(__cpp_if_constexpr) && 201606L <= __cpp_if_constexpr
#  include "serialization/deserialize_if_constexpr.hpp"
#endif

#include <gtest/gtest.h>
#include <ryml.hpp>

#include <string>
#include <vector>

namespace serialization_test {

struct composed {
  template <typename T>
  static serialization::compat::expected<T, std::string> to(
    const ryml::ConstNodeRef &from) {
    return serialization::composed::deserialize.template to<T>(from);
  }
};

#if defined(__cpp_if_constexpr) && 201606L <= __cpp_if_constexpr
struct if_constexpr {
  template <typename T>
  static serialization::compat::expected<T, std::string> to(
    const ryml::ConstNodeRef &from) {
    return serialization::if_constexpr::deserialize.template to<T>(from);
  }
};

using implementations = ::testing::Types<composed, if_constexpr>;
#else
using implementations = ::testing::Types<composed>;
#endif

template <typename Implementation>
class deserialization: public ::testing::Test {
  public:
  using implementation = Implementation;
};

TYPED_TEST_SUITE(deserialization, implementations);

TYPED_TEST(deserialization, reads_a_representative_json_document) {
  const auto result =
    TestFixture::implementation::template to<serialization_data::document>(
      ryml::parse_in_arena(
        c4::to_csubstr(serialization_data::representative_json)));

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

TYPED_TEST(deserialization, reads_the_same_shape_from_yaml) {
  const auto result =
    TestFixture::implementation::template to<serialization_data::document>(
      ryml::parse_in_arena(
        c4::to_csubstr(serialization_data::representative_yaml)));

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

TYPED_TEST(deserialization, reads_a_nested_record) {
  const auto result =
    TestFixture::implementation::template to<serialization_data::nested_record>(
      ryml::parse_in_arena(c4::to_csubstr(serialization_data::nested_json)));

  ASSERT_TRUE(result) << result.error();
  EXPECT_DOUBLE_EQ(23.42, result->ratio);
  EXPECT_EQ("oceanic", result->data.name);
  EXPECT_EQ(815, result->data.code);
}

TYPED_TEST(deserialization, reads_scalars_and_sequences) {
  const auto result =
    TestFixture::implementation::template to<serialization_data::scalar_values>(
      ryml::parse_in_arena(c4::to_csubstr(serialization_data::scalar_json)));

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

TYPED_TEST(deserialization, rejects_an_invalid_nested_integer) {
  const auto result =
    TestFixture::implementation::template to<serialization_data::nested_record>(
      ryml::parse_in_arena(
        c4::to_csubstr(serialization_data::invalid_nested_integer_json)));

  ASSERT_FALSE(result);
  EXPECT_EQ("\"invalid\" is not an integer", result.error());
}

TYPED_TEST(deserialization, rejects_an_unknown_field) {
  const auto result =
    TestFixture::implementation::template to<serialization_data::nested_record>(
      ryml::parse_in_arena(
        c4::to_csubstr(serialization_data::unknown_field_json)));

  ASSERT_FALSE(result);
  EXPECT_EQ("unknown field 'extra'", result.error());
}

TYPED_TEST(deserialization, rejects_a_duplicate_field) {
  const auto result =
    TestFixture::implementation::template to<serialization_data::nested_record>(
      ryml::parse_in_arena(
        c4::to_csubstr(serialization_data::duplicate_field_json)));

  ASSERT_FALSE(result);
  EXPECT_EQ("duplicate field 'ratio'", result.error());
}

TYPED_TEST(deserialization, reports_a_missing_field) {
  const auto result =
    TestFixture::implementation::template to<serialization_data::nested_record>(
      ryml::parse_in_arena(
        c4::to_csubstr(serialization_data::missing_field_json)));

  ASSERT_FALSE(result);
  EXPECT_EQ("missing fields: data", result.error());
}

} // namespace serialization_test
