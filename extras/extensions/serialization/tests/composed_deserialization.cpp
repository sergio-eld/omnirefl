#include "json_match.hpp"

#include <map>
#include <string>
#include <utility>
#include <vector>

#if defined(__GNUC__) && !defined(__clang__)
// GCC warns about GTest's internal conditionals in unbraced assertion guards.
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdangling-else"
#endif

// C++14 registers callable objects; C++11 registers their stateless
// result types.

namespace composed_boolean {

struct value { bool enabled; };

struct read {
  auto operator()(::ryml::ConstNodeRef node) const
    -> omni::compat::expected<value, omni::ryml::issue> {
    const auto parsed = omni::ryml::core::parse_boolean(
      {node.val().str, node.val().len});

    return value{*parsed};
  }
};

} // namespace composed_boolean

#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <>
const auto omni::ryml::cp::to_type<composed_boolean::value> =
  omni::ryml::compose::to_boolean(composed_boolean::read{});
#else
template <>
struct omni::ryml::cp::to_type_t<composed_boolean::value>:
    decltype(omni::ryml::compose::to_boolean(composed_boolean::read{})) {};
#endif

class composed_booleans: public testing::TestWithParam<bool> {};

TEST_P(composed_booleans, deserializes_a_boolean_wrapper) {
  const bool expected = GetParam();
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_boolean::value>{}, expected ? "true" : "false");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_EQ(expected, result.value->enabled);
}

INSTANTIATE_TEST_SUITE_P(values, composed_booleans, testing::Bool());

TEST(composed_boolean, rejects_a_different_shape) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_boolean::value>{}, R"("true")");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (result.diagnostics.issues.size() == 1) {
    const auto &error = result.diagnostics.issues.front();
    EXPECT_EQ(omni::json::issue_code::unexpected_kind, error.reason);
    EXPECT_EQ(omni::json::value_kind::boolean, error.expected);
    EXPECT_TRUE(error.path.empty());
  }
}

namespace composed_integer {

struct value { int count; };

struct read {
  template <typename State>
  auto operator()(State state,
    ::ryml::ConstNodeRef node,
    omni::type_t<value>) const -> omni::ryml::with_diagnostics_t<value> {
    auto result = omni::ryml::core::map_node_to_integral(std::move(state),
      node,
      omni::type_t<int>{});

    return {
      /*value=*/result.value
        ? omni::compat::make_optional(value{*result.value})
        : omni::compat::nullopt,
      /*diagnostics=*/std::move(result.diagnostics),
    };
  }
};

} // namespace composed_integer

#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <>
const auto omni::ryml::cp::to_type<composed_integer::value> =
  omni::ryml::compose::to_integral(composed_integer::read{});
#else
template <>
struct omni::ryml::cp::to_type_t<composed_integer::value>:
    decltype(omni::ryml::compose::to_integral(composed_integer::read{})) {};
#endif

TEST(composed_integer, deserializes_an_integer_wrapper) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_integer::value>{}, "42");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_EQ(42, result.value->count);
}

TEST(composed_integer, reports_conversion_failure) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_integer::value>{}, "99999999999999999999999999999");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (result.diagnostics.issues.size() == 1)
    EXPECT_EQ(omni::json::issue_code::out_of_range,
      result.diagnostics.issues.front().reason);
}

TEST(composed_integer, rejects_a_different_shape) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_integer::value>{}, R"("42")");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (result.diagnostics.issues.size() == 1) {
    const auto &error = result.diagnostics.issues.front();
    EXPECT_EQ(omni::json::issue_code::unexpected_kind, error.reason);
    EXPECT_EQ(omni::json::value_kind::integer, error.expected);
    EXPECT_TRUE(error.path.empty());
  }
}

namespace composed_real {

struct value { double amount; };

struct read {
  auto operator()(::ryml::ConstNodeRef node, value *into) const
    -> omni::compat::expected<omni::compat::monostate, omni::ryml::issue> {
    if (c4::from_chars(node.val(), &into->amount))
      return omni::compat::monostate{};

    return omni::compat::unexpected<omni::ryml::issue>{
      omni::ryml::core::make_issue(omni::json::issue_code::out_of_range,
        omni::json::value_kind::number, node)};
  }
};

} // namespace composed_real

#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <>
const auto omni::ryml::cp::to_type<composed_real::value> =
  omni::ryml::compose::to_floating_point(composed_real::read{});
#else
template <>
struct omni::ryml::cp::to_type_t<composed_real::value>:
    decltype(omni::ryml::compose::to_floating_point(composed_real::read{})) {};
#endif

TEST(composed_real, deserializes_a_real_wrapper_through_a_pointer_callback) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_real::value>{}, "2.5");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_DOUBLE_EQ(2.5, result.value->amount);
}

TEST(composed_real, rejects_a_different_shape) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_real::value>{}, R"("2.5")");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (result.diagnostics.issues.size() == 1) {
    const auto &error = result.diagnostics.issues.front();
    EXPECT_EQ(omni::json::issue_code::unexpected_kind, error.reason);
    EXPECT_EQ(omni::json::value_kind::number, error.expected);
    EXPECT_TRUE(error.path.empty());
  }
}

namespace composed_string {

struct value { std::string text; };

struct read {
  template <typename State>
  auto operator()(State, ::ryml::ConstNodeRef node, omni::type_t<value>) const
    -> omni::compat::expected<value, omni::ryml::issue> {
    return value{node.val().empty()
      ? std::string{} : std::string{node.val().str, node.val().len}};
  }
};

} // namespace composed_string

#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <>
const auto omni::ryml::cp::to_type<composed_string::value> =
  omni::ryml::compose::to_string_like(composed_string::read{});
#else
template <>
struct omni::ryml::cp::to_type_t<composed_string::value>:
    decltype(omni::ryml::compose::to_string_like(composed_string::read{})) {};
#endif

TEST(composed_string, deserializes_a_string_wrapper_with_a_type_tag) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_string::value>{}, R"("gateway")");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_EQ("gateway", result.value->text);
}

TEST(composed_string, rejects_a_different_shape) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_string::value>{}, "7");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (result.diagnostics.issues.size() == 1) {
    const auto &error = result.diagnostics.issues.front();
    EXPECT_EQ(omni::json::issue_code::unexpected_kind, error.reason);
    EXPECT_EQ(omni::json::value_kind::string, error.expected);
    EXPECT_TRUE(error.path.empty());
  }
}

namespace composed_enum {

enum class value { queued, active };

struct read {
  auto operator()(::ryml::ConstNodeRef node) const
    -> omni::compat::expected<value, std::vector<omni::ryml::issue>> {
    if (node.val() == "queued")
      return value::queued;

    if (node.val() == "active")
      return value::active;

    return omni::compat::unexpected<std::vector<omni::ryml::issue>>{{
      omni::ryml::core::make_issue(omni::json::issue_code::invalid_scalar,
        omni::json::value_kind::string, node)}};
  }
};

} // namespace composed_enum

#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <>
const auto omni::ryml::cp::to_type<composed_enum::value> =
  omni::ryml::compose::to_enum(composed_enum::read{});
#else
template <>
struct omni::ryml::cp::to_type_t<composed_enum::value>:
    decltype(omni::ryml::compose::to_enum(composed_enum::read{})) {};
#endif

TEST(composed_enum, deserializes_an_enumerator_through_the_callback) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_enum::value>{}, R"("active")");

  EXPECT_EQ(composed_enum::value::active, result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
}

TEST(composed_enum, reports_an_unrecognized_string) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_enum::value>{}, R"("missing")");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (result.diagnostics.issues.size() == 1)
    EXPECT_EQ(omni::json::issue_code::invalid_scalar,
      result.diagnostics.issues.front().reason);
}

TEST(composed_enum, rejects_a_different_shape) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_enum::value>{}, "7");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (result.diagnostics.issues.size() == 1) {
    const auto &error = result.diagnostics.issues.front();
    EXPECT_EQ(omni::json::issue_code::unexpected_kind, error.reason);
    EXPECT_EQ(omni::json::value_kind::string, error.expected);
    EXPECT_TRUE(error.path.empty());
  }
}

namespace composed_arithmetic {

struct value { double amount; };

struct read {
  auto operator()(::ryml::ConstNodeRef node) const
    -> omni::compat::expected<value, omni::ryml::issue> {
    if (const auto boolean = omni::ryml::core::parse_boolean(
        {node.val().str, node.val().len}))
      return value{*boolean ? 1.0 : 0.0};

    double amount{};
    if (c4::from_chars(node.val(), &amount))
      return value{amount};

    return omni::compat::unexpected<omni::ryml::issue>{
      omni::ryml::core::make_issue(omni::json::issue_code::out_of_range,
        omni::json::value_kind::number, node)};
  }
};

struct example {
  std::string name;
  std::string source;
  double expected;
};

} // namespace composed_arithmetic

#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <>
const auto omni::ryml::cp::to_type<composed_arithmetic::value> =
  omni::ryml::compose::to_arithmetic(composed_arithmetic::read{});
#else
template <>
struct omni::ryml::cp::to_type_t<composed_arithmetic::value>:
    decltype(omni::ryml::compose::to_arithmetic(
      composed_arithmetic::read{})) {};
#endif

class composed_numbers:
    public testing::TestWithParam<composed_arithmetic::example> {};

TEST_P(composed_numbers, deserializes_a_boolean_integer_or_real) {
  const auto &c = GetParam();
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_arithmetic::value>{}, c.source);

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_DOUBLE_EQ(c.expected, result.value->amount);
}

INSTANTIATE_TEST_SUITE_P(shapes,
  composed_numbers,
  testing::Values(
    composed_arithmetic::example{"boolean", "true", 1.0},
    composed_arithmetic::example{"integer", "-7", -7.0},
    composed_arithmetic::example{"real", "2.5", 2.5}),

  [](const testing::TestParamInfo<composed_arithmetic::example> &info) {
    return info.param.name;
  });

TEST(composed_arithmetic, rejects_a_different_shape) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_arithmetic::value>{}, R"("true")");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (result.diagnostics.issues.size() == 1) {
    const auto &error = result.diagnostics.issues.front();
    EXPECT_EQ(omni::json::issue_code::unexpected_kind, error.reason);
    EXPECT_EQ(omni::json::value_kind::scalar, error.expected);
    EXPECT_TRUE(error.path.empty());
  }
}

namespace composed_range {

struct value { std::vector<int> items; };

struct read {
  template <typename State>
  auto operator()(State state, ::ryml::ConstNodeRef node, value *into) const
    -> omni::ryml::with_diagnostics_t<omni::compat::monostate> {
    return omni::ryml::to_range_t{}(std::move(state), node, &into->items);
  }
};

} // namespace composed_range

#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <>
const auto omni::ryml::cp::to_type<composed_range::value> =
  omni::ryml::compose::to_range(composed_range::read{});
#else
template <>
struct omni::ryml::cp::to_type_t<composed_range::value>:
    decltype(omni::ryml::compose::to_range(composed_range::read{})) {};
#endif

TEST(composed_range, deserializes_elements_through_the_forwarded_state) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_range::value>{}, "[7, 15, 42]");
  const std::vector<int> expected{7, 15, 42};

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_EQ(expected, result.value->items);
}

TEST(composed_range, retains_an_element_error_path) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_range::value>{}, R"([7, "bad"])");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (result.diagnostics.issues.size() == 1) {
    const auto &error = result.diagnostics.issues.front();
    EXPECT_EQ(omni::json::issue_code::unexpected_kind, error.reason);
    EXPECT_EQ("/1", omni::ryml::detail::render_path(error.path));
  }
}

TEST(composed_range, rejects_a_different_shape) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_range::value>{}, "{}");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (result.diagnostics.issues.size() == 1) {
    const auto &error = result.diagnostics.issues.front();
    EXPECT_EQ(omni::json::issue_code::unexpected_kind, error.reason);
    EXPECT_EQ(omni::json::value_kind::sequence, error.expected);
    EXPECT_TRUE(error.path.empty());
  }
}

namespace composed_optional {

struct value { omni::compat::optional<composed_integer::value> item; };

struct read {
  template <typename State>
  auto operator()(State state, ::ryml::ConstNodeRef node, value *into) const
    -> omni::ryml::with_diagnostics_t<omni::compat::monostate> {
    return omni::ryml::to_optional_like_t{}(std::move(state),
      node,
      &into->item);
  }
};

} // namespace composed_optional

#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <>
const auto omni::ryml::cp::to_type<composed_optional::value> =
  omni::ryml::compose::to_optional_like(composed_optional::read{});
#else
template <>
struct omni::ryml::cp::to_type_t<composed_optional::value>:
    decltype(omni::ryml::compose::to_optional_like(
      composed_optional::read{})) {};
#endif

TEST(composed_optional, deserializes_a_present_value_through_its_registration) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_optional::value>{}, "42");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_TRUE(result.value->item);
    if (result.value->item)
      EXPECT_EQ(42, result.value->item->count);
  }
}

TEST(composed_optional, deserializes_explicit_null) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_optional::value>{}, "null");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_FALSE(result.value->item);
}

TEST(composed_optional, validates_the_contained_shape_for_quoted_null) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_optional::value>{}, R"("null")");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (result.diagnostics.issues.size() == 1)
    EXPECT_EQ(omni::json::value_kind::integer,
      result.diagnostics.issues.front().expected);
}

namespace composed_record {

struct value {
  int x;
  int y;
};

struct envelope {
  value direct;
  omni::compat::optional<value> maybe;
  std::vector<value> many;
};

struct read {
  template <typename State>
  auto operator()(State state, ::ryml::ConstNodeRef node, value *into) const
    -> omni::ryml::with_diagnostics_t<omni::compat::monostate> {
    auto result = omni::ryml::reflected_scope::to_record_t<>{}(state,
      node,
      into);

    if (result.value && (into->x < 0 || into->y < 0)) {
      result.value.reset();
      result.diagnostics.issues.push_back(omni::ryml::core::make_issue(
        omni::json::issue_code::out_of_range,
        omni::json::value_kind::mapping, node));
    }

    return result;
  }
};

struct optional_example {
  std::string name;
  std::string source;
};

struct invalid_example {
  std::string name;
  std::string source;
  std::string expected_path;
};

} // namespace composed_record

#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <>
const auto omni::ryml::cp::to_type<composed_record::value> =
  omni::ryml::compose::to_record(composed_record::read{});
#else
template <>
struct omni::ryml::cp::to_type_t<composed_record::value>:
    decltype(omni::ryml::compose::to_record(composed_record::read{})) {};
#endif

TEST(composed_record, deserializes_a_record_through_the_forwarded_state) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_record::value>{}, R"({"x": 7, "y": 42})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_EQ(7, result.value->x);
    EXPECT_EQ(42, result.value->y);
  }
}

TEST(composed_record, rejects_a_different_shape) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_record::value>{}, "[]");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (result.diagnostics.issues.size() == 1) {
    const auto &error = result.diagnostics.issues.front();
    EXPECT_EQ(omni::json::issue_code::unexpected_kind, error.reason);
    EXPECT_EQ(omni::json::value_kind::mapping, error.expected);
    EXPECT_TRUE(error.path.empty());
  }
}

TEST(composed_record, deserializes_direct_optional_and_sequence_members) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_record::envelope>{}, R"(
{
  "direct": {"x": 1, "y": 2},
  "maybe": {"x": 3, "y": 4},
  "many": [{"x": 5, "y": 6}, {"x": 7, "y": 8}]
}
)");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_EQ(1, result.value->direct.x);
    EXPECT_EQ(2, result.value->direct.y);
    EXPECT_TRUE(result.value->maybe);
    if (result.value->maybe) {
      EXPECT_EQ(3, result.value->maybe->x);
      EXPECT_EQ(4, result.value->maybe->y);
    }

    EXPECT_EQ(2U, result.value->many.size());
    if (result.value->many.size() == 2) {
      EXPECT_EQ(5, result.value->many[0].x);
      EXPECT_EQ(6, result.value->many[0].y);
      EXPECT_EQ(7, result.value->many[1].x);
      EXPECT_EQ(8, result.value->many[1].y);
    }
  }
}

class composed_record_absence:
    public testing::TestWithParam<composed_record::optional_example> {};

TEST_P(composed_record_absence, permits_an_absent_or_null_optional_record) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_record::envelope>{}, GetParam().source);

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_FALSE(result.value->maybe);
    EXPECT_TRUE(result.value->many.empty());
  }
}

INSTANTIATE_TEST_SUITE_P(documents,
  composed_record_absence,
  testing::Values(
    composed_record::optional_example{
      "absent", R"({"direct": {"x": 1, "y": 2}, "many": []})"},

    composed_record::optional_example{
      "null", R"({"direct": {"x": 1, "y": 2}, "maybe": null, "many": []})"}),

  [](const testing::TestParamInfo<composed_record::optional_example> &info) {
    return info.param.name;
  });

class composed_record_validation:
    public testing::TestWithParam<composed_record::invalid_example> {};

TEST_P(composed_record_validation, retains_a_nested_custom_validation_error) {
  const auto &c = GetParam();
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_record::envelope>{}, c.source);

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (result.diagnostics.issues.size() == 1) {
    const auto &error = result.diagnostics.issues.front();
    EXPECT_EQ(omni::json::issue_code::out_of_range, error.reason);
    EXPECT_EQ(c.expected_path, omni::ryml::detail::render_path(error.path));
  }
}

INSTANTIATE_TEST_SUITE_P(members,
  composed_record_validation,
  testing::Values(
    composed_record::invalid_example{"optional", R"(
{
  "direct": {"x": 1, "y": 2},
  "maybe": {"x": -3, "y": 4},
  "many": []
}
)",

      "/maybe"},

    composed_record::invalid_example{"sequence", R"(
{
  "direct": {"x": 1, "y": 2},
  "maybe": null,
  "many": [{"x": -5, "y": 6}]
}
)",

      "/many/0"}),

  [](const testing::TestParamInfo<composed_record::invalid_example> &info) {
    return info.param.name;
  });

namespace composed_existing {

struct value {
  int id;

  value() = delete;
  explicit value(int id): id(id) {}
};

struct envelope { value item{0}; };

} // namespace composed_existing

#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <>
const auto omni::ryml::cp::to_type<composed_existing::value> =
  omni::ryml::compose::to_record(
    omni::ryml::reflected_scope::to_record_t<>{});
#else
template <>
struct omni::ryml::cp::to_type_t<composed_existing::value>:
    decltype(omni::ryml::compose::to_record(
      omni::ryml::reflected_scope::to_record_t<>{})) {};
#endif

TEST(composed_existing, deserializes_into_a_non_default_constructible_root) {
  composed_existing::value into{7};
  const auto result = serialization_test::deserialize(&into, R"({"id": 42})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  EXPECT_EQ(42, into.id);
}

TEST(composed_existing,
  deserializes_a_direct_non_default_constructible_member) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_existing::envelope>{}, R"({"item": {"id": 42}})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_EQ(42, result.value->item.id);
}

TEST(composed_existing, deserializes_into_an_existing_parent_record) {
  composed_existing::envelope into{};
  const auto result = serialization_test::deserialize(
    &into, R"({"item": {"id": 42}})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  EXPECT_EQ(42, into.item.id);
}

namespace composed_members {

struct value {
  composed_boolean::value flag;
  composed_integer::value count;
  composed_real::value price;
  composed_string::value label;
  composed_enum::value status;
  composed_arithmetic::value score;
  composed_range::value items;
  composed_optional::value maybe;
  composed_record::value point;
};

} // namespace composed_members

TEST(composed_members, deserializes_every_registered_shape_as_a_member) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_members::value>{}, R"(
{
  "flag": true,
  "count": 42,
  "price": 2.5,
  "label": "gateway",
  "status": "active",
  "score": 7,
  "items": [1, 2],
  "maybe": 15,
  "point": {"x": 3, "y": 4}
}
)");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_TRUE(result.value->flag.enabled);
    EXPECT_EQ(42, result.value->count.count);
    EXPECT_DOUBLE_EQ(2.5, result.value->price.amount);
    EXPECT_EQ("gateway", result.value->label.text);
    EXPECT_EQ(composed_enum::value::active, result.value->status);
    EXPECT_DOUBLE_EQ(7, result.value->score.amount);
    EXPECT_EQ((std::vector<int>{1, 2}), result.value->items.items);
    EXPECT_TRUE(result.value->maybe.item);
    if (result.value->maybe.item)
      EXPECT_EQ(15, result.value->maybe.item->count);

    EXPECT_EQ(3, result.value->point.x);
    EXPECT_EQ(4, result.value->point.y);
  }
}

namespace composed_state_expected {

struct value { bool enabled; };

struct read {
  OMNI_CONSTRAINED_TEMPLATE((typename State),
    (omni::ryml::is_read_state<State>::value))
  auto operator()(State, ::ryml::ConstNodeRef node) const
    -> omni::compat::expected<value, omni::ryml::issue> {
    const auto parsed = omni::ryml::core::parse_boolean(
      {node.val().str, node.val().len});

    return value{*parsed};
  }
};

} // namespace composed_state_expected

#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <>
const auto omni::ryml::cp::to_type<composed_state_expected::value> =
  omni::ryml::compose::to_boolean(composed_state_expected::read{});
#else
template <>
struct omni::ryml::cp::to_type_t<composed_state_expected::value>:
    decltype(omni::ryml::compose::to_boolean(
      composed_state_expected::read{})) {};
#endif

TEST(composed_state_expected, deserializes_with_node_and_state_arguments) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_state_expected::value>{}, "true");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_TRUE(result.value->enabled);
}

namespace composed_state_diagnostics {

struct value { bool enabled; };
struct envelope { value flag; };

struct read {
  OMNI_CONSTRAINED_TEMPLATE((typename State),
    (omni::ryml::is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node) const
    -> omni::ryml::with_diagnostics_t<value> {
    auto result = omni::ryml::core::map_node_to_boolean(std::move(state),
      node,
      omni::type_t<bool>{});

    return {
      /*value=*/result.value
        ? omni::compat::make_optional(value{*result.value})
        : omni::compat::nullopt,
      /*diagnostics=*/std::move(result.diagnostics),
    };
  }
};

} // namespace composed_state_diagnostics

#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <>
const auto omni::ryml::cp::to_type<composed_state_diagnostics::value> =
  omni::ryml::compose::to_boolean(composed_state_diagnostics::read{});
#else
template <>
struct omni::ryml::cp::to_type_t<composed_state_diagnostics::value>:
    decltype(omni::ryml::compose::to_boolean(
      composed_state_diagnostics::read{})) {};
#endif

TEST(composed_state_diagnostics, deserializes_a_nested_value_with_diagnostics) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_state_diagnostics::envelope>{}, R"({"flag": false})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_FALSE(result.value->flag.enabled);
}

TEST(composed_state_diagnostics, retains_the_nested_path_on_a_shape_error) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_state_diagnostics::envelope>{},
    R"({"flag": "false"})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (result.diagnostics.issues.size() == 1) {
    const auto &error = result.diagnostics.issues.front();
    EXPECT_EQ(omni::json::issue_code::unexpected_kind, error.reason);
    EXPECT_EQ("/flag", omni::ryml::detail::render_path(error.path));
  }
}

namespace composed_value_precedence {

struct value { bool enabled; };
struct envelope { value flag; };

struct read {
  template <typename State>
  auto operator()(State, ::ryml::ConstNodeRef node, omni::type_t<value>) const
    -> omni::compat::expected<value, omni::ryml::issue> {
    const auto parsed = omni::ryml::core::parse_boolean(
      {node.val().str, node.val().len});

    if (!*parsed)
      return omni::compat::unexpected<omni::ryml::issue>{
        omni::ryml::core::make_issue(omni::json::issue_code::out_of_range,
          omni::json::value_kind::boolean, node)};

    return value{*parsed};
  }

  auto operator()(::ryml::ConstNodeRef node, value *) const
    -> omni::compat::expected<omni::compat::monostate, omni::ryml::issue> {
    return omni::compat::unexpected<omni::ryml::issue>{
      omni::ryml::core::make_issue(omni::json::issue_code::invalid_scalar,
        omni::json::value_kind::boolean, node)};
  }
};

} // namespace composed_value_precedence

#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <>
const auto omni::ryml::cp::to_type<composed_value_precedence::value> =
  omni::ryml::compose::to_boolean(composed_value_precedence::read{});
#else
template <>
struct omni::ryml::cp::to_type_t<composed_value_precedence::value>:
    decltype(omni::ryml::compose::to_boolean(
      composed_value_precedence::read{})) {};
#endif

TEST(composed_value_precedence, prefers_type_tag_when_creating_a_root_value) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_value_precedence::value>{}, "true");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_TRUE(result.value->enabled);
}

TEST(composed_value_precedence, prefers_type_tag_for_an_existing_destination) {
  composed_value_precedence::value into{false};
  const auto result = serialization_test::deserialize(&into, "true");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  EXPECT_TRUE(into.enabled);
}

TEST(composed_value_precedence, prefers_type_tag_for_a_record_member) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_value_precedence::envelope>{}, R"({"flag": true})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_TRUE(result.value->flag.enabled);
}

TEST(composed_value_precedence,
  does_not_fall_back_after_a_value_mapping_error) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_value_precedence::value>{}, "false");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (result.diagnostics.issues.size() == 1)
    EXPECT_EQ(omni::json::issue_code::out_of_range,
      result.diagnostics.issues.front().reason);
}

namespace composed_move_only {

struct value {
  bool enabled{};

  value() = default;
  explicit value(bool enabled): enabled(enabled) {}
  value(const value &) = delete;
  value &operator=(const value &) = delete;
  value(value &&) = default;
  value &operator=(value &&) = default;
};

struct envelope {
  value flag;
  unsigned bits: 3;
};

struct read {
  auto operator()(::ryml::ConstNodeRef node) const
    -> omni::compat::expected<value, omni::ryml::issue> {
    const auto parsed = omni::ryml::core::parse_boolean(
      {node.val().str, node.val().len});

    return value{*parsed};
  }
};

} // namespace composed_move_only

#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <>
const auto omni::ryml::cp::to_type<composed_move_only::value> =
  omni::ryml::compose::to_boolean(composed_move_only::read{});
#else
template <>
struct omni::ryml::cp::to_type_t<composed_move_only::value>:
    decltype(omni::ryml::compose::to_boolean(composed_move_only::read{})) {};
#endif

TEST(composed_move_only, assigns_produced_values_directly_to_record_fields) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_move_only::envelope>{},
    R"({"flag": true, "bits": 5})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_TRUE(result.value->flag.enabled);
    EXPECT_EQ(5U, result.value->bits);
  }
}

namespace composed_rvalue_state {

struct value { bool enabled; };
struct envelope { value flag; };

struct read {
  template <typename Config>
  auto operator()(omni::ryml::read_state_t<Config> &&,
    ::ryml::ConstNodeRef node,
    omni::type_t<value>) const
    -> omni::compat::expected<value, omni::ryml::issue> {
    const auto parsed = omni::ryml::core::parse_boolean(
      {node.val().str, node.val().len});

    return value{*parsed};
  }
};

} // namespace composed_rvalue_state

#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <>
const auto omni::ryml::cp::to_type<composed_rvalue_state::value> =
  omni::ryml::compose::to_boolean(composed_rvalue_state::read{});
#else
template <>
struct omni::ryml::cp::to_type_t<composed_rvalue_state::value>:
    decltype(omni::ryml::compose::to_boolean(
      composed_rvalue_state::read{})) {};
#endif

TEST(composed_rvalue_state, deserializes_a_root_value) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_rvalue_state::value>{}, "true");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_TRUE(result.value->enabled);
}

TEST(composed_rvalue_state, deserializes_a_nested_value) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_rvalue_state::envelope>{}, R"({"flag": false})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_FALSE(result.value->flag.enabled);
}

namespace composed_constructed {

struct value {
  int number;

  explicit value(int number): number(number) {}
  value(const value &) = delete;
  value(value &&) = default;
  value &operator=(value &&) = default;
};

struct read {
  auto operator()(::ryml::ConstNodeRef node) const
    -> omni::compat::expected<value, omni::ryml::issue> {
    int number{};
    if (c4::from_chars(node.val(), &number))
      return value{number};

    return omni::compat::unexpected<omni::ryml::issue>{
      omni::ryml::core::make_issue(omni::json::issue_code::out_of_range,
        omni::json::value_kind::integer, node)};
  }
};

struct envelope {
  std::vector<value> values;
  std::map<std::string, value> indexed;
  omni::compat::optional<value> item;
};

} // namespace composed_constructed

// The C++11 registration form remains usable in every supported standard.
template <>
struct omni::ryml::cp::to_type_t<composed_constructed::value>:
    decltype(omni::ryml::compose::to_integral(composed_constructed::read{})) {};

TEST(composed_constructed, constructs_values_inside_ranges_and_optionals) {
  const auto result = serialization_test::deserialize(
    omni::type_t<composed_constructed::envelope>{},
    R"({"values": [7, 8], "indexed": {"port": 9}, "item": 10})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (!result.value)
    return;

  EXPECT_EQ(2U, result.value->values.size());
  if (result.value->values.size() == 2) {
    EXPECT_EQ(7, result.value->values[0].number);
    EXPECT_EQ(8, result.value->values[1].number);
  }

  EXPECT_EQ(1U, result.value->indexed.size());
  if (result.value->indexed.size() == 1)
    EXPECT_EQ(9, result.value->indexed.begin()->second.number);

  EXPECT_TRUE(result.value->item);
  if (result.value->item)
    EXPECT_EQ(10, result.value->item->number);
}

// FIXME: [high] External: MSVC STL cannot grow vector<map<string,
// optional<move-only>>>: vector attempts to copy the map during relocation.
// A standard-only reproduction is vector<map<string, unique_ptr<int>>>;
// v.emplace_back(). https://github.com/microsoft/STL/issues/5552
#if !defined(_MSVC_STL_VERSION)
TEST(composed_constructed, discovers_values_through_root_compounds) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::vector<std::map<std::string,
      omni::compat::optional<composed_constructed::value>>>>{},
    R"([{"present": 7, "empty": null}])");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (!result.value)
    return;

  EXPECT_EQ(1U, result.value->size());
  if (result.value->size() != 1)
    return;

  const auto &values = result.value->front();
  EXPECT_EQ(2U, values.size());
  const auto present = values.find("present");
  EXPECT_NE(values.end(), present);
  if (present != values.end()) {
    EXPECT_TRUE(present->second);
    if (present->second)
      EXPECT_EQ(7, present->second->number);
  }

  const auto empty = values.find("empty");
  EXPECT_NE(values.end(), empty);
  if (empty != values.end())
    EXPECT_FALSE(empty->second);
}
#endif

namespace composed_bound_input {

struct value {
  int bias;
  int number;
};

struct envelope { value entry; };

struct read {
  template <typename State>
  auto operator()(State state, ::ryml::ConstNodeRef node, value *into) const
    -> omni::ryml::with_diagnostics_t<omni::compat::monostate> {
    return omni::ryml::core::map_node_to_integral(std::move(state), node,
      omni::type_t<int>{})
      .transform([into](int number) -> omni::compat::monostate {
        into->number = number + into->bias;
        return {};
      });
  }
};

} // namespace composed_bound_input

template <>
struct omni::ryml::cp::to_type_t<composed_bound_input::value>:
    decltype(omni::ryml::compose::to_integral(composed_bound_input::read{})) {};

TEST(composed_bound_input, supplies_existing_member_data_to_a_pointer_mapping) {
  composed_bound_input::envelope into{{42, 1}};
  const auto result = serialization_test::deserialize(&into, R"({"entry": 7})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  EXPECT_EQ(49, into.entry.number);
}

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
