#include "json_match.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#if defined(__GNUC__) && !defined(__clang__)
// GCC warns about GTest's internal conditionals in unbraced assertion guards.
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdangling-else"
#endif

namespace {

using omni::json::issue_code;
using omni::json::value_kind;
using node_check = omni::ryml::with_diagnostics_t<omni::compat::monostate> (*)(
  ::ryml::ConstNodeRef);

struct node_check_case {
  std::string name;
  std::string source;
  node_check check;
  // Empty means success; otherwise the required category in the diagnostic.
  omni::compat::optional<value_kind> expected;
};

class node_checks: public testing::TestWithParam<node_check_case> {};

TEST_P(node_checks, reports_the_required_category) {
  const auto &c = GetParam();
  const auto tree = omni::ryml::core::parse(c.source);
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto result = c.check(tree->crootref());
  EXPECT_EQ(!c.expected, bool(result.value));
  EXPECT_EQ(c.expected ? 1U : 0U, result.diagnostics.issues.size());
  if (c.expected) {
    for (const auto &entry : result.diagnostics.issues) {
      EXPECT_EQ(issue_code::unexpected_kind, entry.reason);
      EXPECT_EQ(*c.expected, entry.expected);
      EXPECT_EQ(tree->crootref().id(), entry.node_id);
      EXPECT_TRUE(entry.path.empty());
    }
  }
}

using namespace omni::ryml::core;

INSTANTIATE_TEST_SUITE_P(shapes,
  node_checks,
  testing::Values(
    node_check_case{"object", "{}", expect_object_node, {}},
    node_check_case{"array", "[]", expect_array_node, {}},
    node_check_case{"string", R"("text")", expect_string_node, {}},
    node_check_case{"yaml_string", "text", expect_string_node, {}},
    node_check_case{"tagged_string", "!!str null", expect_string_node, {}},
    node_check_case{"quoted_null", R"("null")", expect_string_node, {}},
    node_check_case{"boolean", "true", expect_boolean_node, {}},
    node_check_case{"false_boolean", "false", expect_boolean_node, {}},
    node_check_case{"yaml_boolean", "True", expect_boolean_node, {}},
    node_check_case{"integer", "-7", expect_integer_node, {}},
    node_check_case{"real", "2.5", expect_number_node, {}},
    node_check_case{"integer_as_number", "7", expect_number_node, {}},
    node_check_case{"null_value", "null", expect_value_node, {}},
    node_check_case{"object_value", "{}", expect_value_node, {}},
    node_check_case{"array_value", "[]", expect_value_node, {}},
    node_check_case{"scalar", "null", expect_scalar_node, {}},
    node_check_case{"array_not_object", "[]", expect_object_node,
      value_kind::mapping},
    node_check_case{"object_not_array", "{}", expect_array_node,
      value_kind::sequence},
    node_check_case{"object_not_scalar", "{}", expect_scalar_node,
      value_kind::scalar},
    node_check_case{"number_not_string", "7", expect_string_node,
      value_kind::string},
    node_check_case{"boolean_not_string", "true", expect_string_node,
      value_kind::string},
    node_check_case{"null_not_string", "null", expect_string_node,
      value_kind::string},
    node_check_case{"quoted_boolean", R"("true")", expect_boolean_node,
      value_kind::boolean},
    node_check_case{"tagged_boolean_as_string", "!!str true",
      expect_boolean_node, value_kind::boolean},
    node_check_case{"numeric_boolean", "1", expect_boolean_node,
      value_kind::boolean},
    node_check_case{"quoted_integer", R"("7")", expect_integer_node,
      value_kind::integer},
    node_check_case{"real_not_integer", "2.5", expect_integer_node,
      value_kind::integer},
    node_check_case{"quoted_real", R"("2.5")", expect_number_node,
      value_kind::number}),

  [](const testing::TestParamInfo<node_check_case> &c) {
    return c.param.name;
  });

struct counted_record { std::size_t count; };
struct counted_envelope { counted_record inner; };

omni::compat::expected<counted_record, issue_code> count_properties(
  ::ryml::ConstNodeRef node) {
  const auto count = node.num_children();
  if (count > 2)
    return omni::compat::unexpected<issue_code>{issue_code::out_of_range};

  return counted_record{count};
}

omni::compat::expected<counted_record, omni::ryml::issue> reject_count(
  ::ryml::ConstNodeRef node) {
  const auto field = node["count"];
  omni::ryml::issue error{};
  error.reason = issue_code::out_of_range;
  error.expected = value_kind::integer;
  error.actual = value_kind::integer;
  error.path.push_back({field.key(), omni::compat::nullopt});
  error.node_id = field.id();
  return omni::compat::unexpected<omni::ryml::issue>{std::move(error)};
}

} // namespace

// C++14 and later can register the composed callable directly.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <>
const auto omni::ryml::cp::to_type<counted_record> =
  omni::ryml::reflected_scope::to_record(count_properties);
#else
// C++11 initializes the function-pointer binding when the mapping is called.
template <>
struct omni::ryml::cp::to_type_t<counted_record> {
  OMNI_CONSTRAINED_TEMPLATE((typename State),
    (omni::ryml::is_read_state<State>::value))
  auto operator()(::ryml::ConstNodeRef node, counted_record *into,
    State state) const -> omni::ryml::with_diagnostics_t<
      omni::compat::monostate> {
    return omni::ryml::reflected_scope::to_record(count_properties)(
      node, into, state);
  }
};
#endif

namespace {

struct callback_case {
  std::string name;
  std::string source;
  omni::compat::optional<std::size_t> count;
  omni::compat::optional<issue_code> error;
};

class record_callback: public testing::TestWithParam<callback_case> {};

TEST_P(record_callback, maps_through_registration_with_node_validation) {
  const auto &c = GetParam();
  const auto result = serialization_test::deserialize(
    omni::type_t<counted_record>{}, c.source);

  EXPECT_EQ(bool(c.count), bool(result.value));
  if (result.value && c.count)
    EXPECT_EQ(*c.count, result.value->count);

  EXPECT_EQ(c.error ? 1U : 0U, result.diagnostics.issues.size());
  if (c.error) {
    for (const auto &entry : result.diagnostics.issues) {
      EXPECT_EQ(*c.error, entry.reason);
      EXPECT_TRUE(entry.path.empty());
    }
  }
}

INSTANTIATE_TEST_SUITE_P(objects,
  record_callback,
  testing::Values(
    callback_case{"empty", "{}", 0U, {}},
    callback_case{"one_property", R"({"first": 1})", 1U, {}},
    callback_case{"wrong_shape", "[]", {}, issue_code::unexpected_kind},
    callback_case{"rejected_value", R"({"a": 1, "b": 2, "c": 3})", {},
      issue_code::out_of_range}),

  [](const testing::TestParamInfo<callback_case> &c) { return c.param.name; });

TEST(record_callback, preserves_the_nested_path_on_a_shape_mismatch) {
  const auto result = serialization_test::deserialize(
    omni::type_t<counted_envelope>{}, R"({"inner": []})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  for (const auto &entry : result.diagnostics.issues) {
    EXPECT_EQ(issue_code::unexpected_kind, entry.reason);
    EXPECT_EQ(value_kind::mapping, entry.expected);
    EXPECT_EQ("/inner", omni::ryml::detail::render_path(entry.path));
  }
}

TEST(record_callback, supports_value_and_existing_destination_overloads) {
  const auto tree = omni::ryml::core::parse(R"({"first": 1})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const omni::ryml::config_t<> options{};
  const omni::ryml::read_state_t<> state{options, {}, 0, 0};
  const auto mapping = omni::ryml::reflected_scope::to_record(count_properties);
  const auto value = mapping(tree->crootref(), omni::type_t<counted_record>{},
    state);

  counted_record into{};
  const auto assigned = mapping(tree->crootref(), &into, state);
  EXPECT_TRUE(value.value);
  if (value.value)
    EXPECT_EQ(1U, value.value->count);

  EXPECT_TRUE(assigned.value);
  EXPECT_EQ(1U, into.count);
}

TEST(record_callback, returns_no_new_issues_after_the_error_budget_is_exceeded) {
  const auto tree = omni::ryml::core::parse("[]");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  omni::ryml::config_t<> options{};
  options.error_policy.error_limit = 0;
  const omni::ryml::read_state_t<> state{
    options, {}, options.error_policy.error_limit + 1, 0};
  const auto mapping = omni::ryml::reflected_scope::to_record(count_properties);
  const auto result = mapping(tree->crootref(), omni::type_t<counted_record>{},
    state);

  EXPECT_FALSE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
}

TEST(record_callback, prefixes_relative_issue_paths_and_retains_source_nodes) {
  const auto tree = omni::ryml::core::parse(R"({"outer": {"count": 999}})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const omni::ryml::config_t<> options{};
  const omni::ryml::read_state_t<> parent{options, {}, 0, 0};
  const auto outer = tree->crootref()["outer"];
  const auto state = parent.child({outer.key(), omni::compat::nullopt});
  const auto mapping = omni::ryml::reflected_scope::to_record(reject_count);
  const auto result = mapping(outer, omni::type_t<counted_record>{}, state);

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  for (const auto &entry : result.diagnostics.issues) {
    EXPECT_EQ(issue_code::out_of_range, entry.reason);
    EXPECT_EQ("/outer/count", omni::ryml::detail::render_path(entry.path));
    EXPECT_EQ(outer["count"].id(), entry.node_id);
  }
}

} // namespace

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
