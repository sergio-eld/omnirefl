#include "json_match.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
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
using omni::ryml::compose::node_check;
using omni::ryml::compose::to_record;
using omni::ryml::compose::with_node_check;
using serialization_test::map_using;

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
  EXPECT_EQ(!bool(c.expected), bool(result));
  if (c.expected && !result) {
    EXPECT_EQ(issue_code::unexpected_kind, result.error().reason);
    EXPECT_EQ(*c.expected, result.error().expected);
    EXPECT_EQ(tree->crootref().id(), result.error().node_id);
    EXPECT_TRUE(result.error().path.empty());
  }
}

TEST_P(node_checks, reports_the_nodes_document_path) {
  const auto &c = GetParam();
  const auto tree = omni::ryml::core::parse(
    "records:\n  - payload: " + c.source + "\n");

  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto node = tree->crootref()["records"][0]["payload"];
  const auto result = c.check(node);

  EXPECT_EQ(!bool(c.expected), bool(result));
  if (result) {
    EXPECT_EQ(node.id(), result->id());
    EXPECT_EQ(node.tree(), result->tree());
  } else {
    EXPECT_EQ("/records/0/payload",
      omni::ryml::detail::render_path(result.error().path));

    EXPECT_EQ(node.id(), result.error().node_id);
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
struct copy_only_record {
  int value;

  copy_only_record() = default;
  copy_only_record(const copy_only_record &) = default;
  copy_only_record(copy_only_record &&) = delete;
};

omni::compat::expected<counted_record, omni::ryml::issue> count_properties(
  ::ryml::ConstNodeRef node) {
  const auto count = node.num_children();
  if (count > 2)
    return omni::compat::unexpected<omni::ryml::issue>{
      omni::ryml::core::make_issue(issue_code::out_of_range,
        value_kind::unknown, node)};

  return counted_record{count};
}

omni::compat::expected<counted_record, omni::ryml::issue> reject_count(
  ::ryml::ConstNodeRef node) {
  const auto field = node["count"];
  auto error = omni::ryml::core::make_issue(issue_code::out_of_range,
    value_kind::integer, field);

  return omni::compat::unexpected<omni::ryml::issue>{std::move(error)};
}

struct constant_mapping {
  OMNI_CONSTRAINED_TEMPLATE((typename State),
    (omni::ryml::is_read_state<State>::value))
  auto operator()(State, ::ryml::ConstNodeRef, omni::type_t<int>) const
    -> omni::ryml::with_diagnostics_t<int>;

  auto operator()(int, ::ryml::ConstNodeRef, omni::type_t<int>) const
    -> omni::ryml::with_diagnostics_t<int>;
};

struct owned_value_callback {
  OMNI_CONSTRAINED_TEMPLATE((typename State),
    (omni::ryml::is_read_state<State>::value
      && !std::is_lvalue_reference<State>::value))
  auto operator()(State &&, ::ryml::ConstNodeRef node,
    omni::type_t<counted_record>) const
    -> omni::compat::expected<counted_record, omni::ryml::issue> {
    return count_properties(node);
  }
};

struct owned_state_callback {
  OMNI_CONSTRAINED_TEMPLATE((typename State),
    (omni::ryml::is_read_state<State>::value
      && !std::is_lvalue_reference<State>::value))
  auto operator()(State &&, ::ryml::ConstNodeRef node) const
    -> omni::compat::expected<counted_record, omni::ryml::issue> {
    return count_properties(node);
  }
};

struct owned_mutating_callback {
  OMNI_CONSTRAINED_TEMPLATE((typename State),
    (omni::ryml::is_read_state<State>::value
      && !std::is_lvalue_reference<State>::value))
  auto operator()(State &&, ::ryml::ConstNodeRef node,
    counted_record *into) const
    -> omni::compat::expected<omni::compat::monostate, omni::ryml::issue> {
    into->count = node.num_children();
    return omni::compat::monostate{};
  }
};

struct parse_integer_callback {
  OMNI_CONSTRAINED_TEMPLATE((typename State),
    (omni::ryml::is_read_state<State>::value))
  auto operator()(State, ::ryml::ConstNodeRef node, omni::type_t<int>) const
    -> omni::compat::expected<int, omni::ryml::issue> {
    int value{};
    if (c4::from_chars(node.val(), &value))
      return value;

    return omni::compat::unexpected<omni::ryml::issue>{
      omni::ryml::core::make_issue(issue_code::out_of_range,
        value_kind::unknown, node)};
  }
};

struct nonassignable_account {
  const int id;
  std::size_t fields;
};

struct map_nonassignable_account {
  OMNI_CONSTRAINED_TEMPLATE((typename State),
    (omni::ryml::is_read_state<State>::value))
  auto operator()(const State &state, ::ryml::ConstNodeRef node,
    nonassignable_account *into) const
    -> omni::compat::expected<omni::compat::monostate, omni::ryml::issue> {
    if (state.config.error_policy.unknown_fields
        == omni::json::unknown_policy::reject
      && node.has_child("name"))
      return omni::compat::unexpected<omni::ryml::issue>{
        omni::ryml::core::make_issue(issue_code::unknown_field,
          value_kind::unknown, node)};

    into->fields = node.num_children();
    return omni::compat::monostate{};
  }
};

struct diagnostic_count_callback {
  OMNI_CONSTRAINED_TEMPLATE((typename State),
    (omni::ryml::is_read_state<State>::value))
  auto operator()(const State &state, ::ryml::ConstNodeRef node,
    omni::type_t<counted_record>) const
    -> omni::ryml::with_diagnostics_t<counted_record> {
    return {
      /*value=*/counted_record{node.num_children()},
      /*diagnostics=*/{{state.config.error_policy, {}}, {}, {}},
    };
  }
};

} // namespace

// C++14 and later can register the composed callable directly.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <>
const auto omni::ryml::cp::to_type<counted_record> =
  to_record(count_properties);
#else
// C++11 initializes the function-pointer binding when the mapping is called.
template <>
struct omni::ryml::cp::to_type_t<counted_record> {
  OMNI_CONSTRAINED_TEMPLATE((typename State),
    (omni::ryml::is_read_state<State>::value))
  auto operator()(State state,
    ::ryml::ConstNodeRef node,
    counted_record *into) const -> omni::ryml::with_diagnostics_t<
      omni::compat::monostate> {
    return to_record(count_properties)(std::move(state), node, into);
  }
};
#endif

namespace {

TEST(record_mapping, constructs_a_copy_only_value_with_the_default_mapping) {
  const auto result = serialization_test::deserialize(
    omni::type_t<copy_only_record>{}, R"({"value": 42})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_EQ(42, result.value->value);
}

TEST(arithmetic_mapping, constrains_value_and_mutating_destinations) {
  using omni::ryml::default_to_arithmetic;
  using state = omni::ryml::read_state_t<>;
  using node = ::ryml::ConstNodeRef;

  EXPECT_TRUE((omni::compat::is_invocable<decltype(map_node_to_arithmetic),
    state, node, omni::type_t<bool>>::value));
  EXPECT_FALSE((omni::compat::is_invocable<decltype(map_node_to_arithmetic),
    state, node, omni::type_t<const bool>>::value));
  EXPECT_TRUE((omni::compat::is_invocable<decltype(default_to_arithmetic),
    state, node, int *>::value));
  EXPECT_FALSE((omni::compat::is_invocable<decltype(default_to_arithmetic),
    state, node, const int *>::value));
  EXPECT_FALSE((omni::compat::is_invocable<decltype(default_to_arithmetic),
    state, node, omni::type_t<const bool>>::value));
}

TEST(node_check_adapter, requires_a_read_state) {
  const auto mapping = with_node_check<expect_object_node>(constant_mapping{});

  EXPECT_TRUE((omni::compat::is_invocable<decltype(mapping),
    omni::ryml::read_state_t<>, ::ryml::ConstNodeRef,
    omni::type_t<int>>::value));
  EXPECT_FALSE((omni::compat::is_invocable<decltype(mapping),
    int, ::ryml::ConstNodeRef, omni::type_t<int>>::value));
}

auto expect_nonempty_object(::ryml::ConstNodeRef node)
  -> omni::compat::expected<::ryml::ConstNodeRef, omni::ryml::issue> {
  if (node.is_map() && node.num_children())
    return node;

  omni::ryml::issue error{};
  error.reason = issue_code::out_of_range;
  error.expected = value_kind::mapping;
  error.actual = value_kind::unknown;
  return omni::compat::unexpected<omni::ryml::issue>{std::move(error)};
}

TEST(node_check_adapter, attributes_a_custom_check_failure_to_the_input_node) {
  const auto tree = omni::ryml::core::parse(R"({"payload": {}})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const omni::ryml::config_t<> options{};
  const auto node = tree->crootref()["payload"];
  const auto mapping = with_node_check<expect_nonempty_object>(
    omni::ryml::compose::lift_to_with_diagnostics(count_properties));

  const auto result = map_using(options, mapping,
    omni::type_t<counted_record>{}, node);

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  for (const auto &entry : result.diagnostics.issues) {
    EXPECT_EQ(issue_code::out_of_range, entry.reason);
    EXPECT_EQ(node.id(), entry.node_id);
    EXPECT_EQ("/payload", omni::ryml::detail::render_path(entry.path));
    EXPECT_EQ(value_kind::mapping, entry.actual);
  }
}

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

TEST(record_callback, passes_owned_state_to_a_value_callback) {
  const auto tree = omni::ryml::core::parse(R"({"first": 1})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto mapping = to_record(owned_value_callback{});

  const omni::ryml::config_t<> options{};
  const auto result = map_using(options, mapping,
    omni::type_t<counted_record>{}, tree->crootref());

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_EQ(1U, result.value->count);
}

TEST(record_callback, passes_owned_state_to_a_state_only_callback) {
  const auto tree = omni::ryml::core::parse(R"({"first": 1})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto mapping = to_record(owned_state_callback{});

  const omni::ryml::config_t<> options{};
  const auto result = map_using(options, mapping,
    omni::type_t<counted_record>{}, tree->crootref());

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_EQ(1U, result.value->count);
}

TEST(record_callback, passes_owned_state_to_a_mutating_callback) {
  const auto tree = omni::ryml::core::parse(R"({"first": 1})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto mapping = to_record(owned_mutating_callback{});

  const omni::ryml::config_t<> options{};
  counted_record into{};
  const auto result = map_using(options, mapping,
    &into, tree->crootref());

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  EXPECT_EQ(1U, into.count);
}

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

TEST(record_callback, lifts_the_callback_without_explicit_adaptation) {
  const auto tree = omni::ryml::core::parse(R"({"first": 1})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const omni::ryml::config_t<> options{};
  const auto mapping = to_record(count_properties);
  const auto value = map_using(options, mapping,
    omni::type_t<counted_record>{}, tree->crootref());

  counted_record into{};
  const auto assigned = map_using(options, mapping, &into, tree->crootref());
  EXPECT_TRUE(value.value);
  if (value.value)
    EXPECT_EQ(1U, value.value->count);

  EXPECT_TRUE(value.diagnostics.issues.empty());
  EXPECT_TRUE(assigned.value);
  EXPECT_EQ(1U, into.count);
  EXPECT_TRUE(assigned.diagnostics.issues.empty());
}

TEST(record_callback, retains_document_paths_and_source_nodes) {
  const auto tree = omni::ryml::core::parse(R"({"outer": {"count": 999}})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const omni::ryml::config_t<> options{};
  const auto outer = tree->crootref()["outer"];
  const auto mapping = to_record(reject_count);
  const auto result = map_using(options, mapping,
    omni::type_t<counted_record>{}, outer);

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  for (const auto &entry : result.diagnostics.issues) {
    EXPECT_EQ(issue_code::out_of_range, entry.reason);
    EXPECT_EQ("/outer/count", omni::ryml::detail::render_path(entry.path));
    EXPECT_EQ(outer["count"].id(), entry.node_id);
  }
}

TEST(record_callback, preserves_multiple_issues_and_their_document_paths) {
  const auto tree = omni::ryml::core::parse(
    R"({"outer": {"count": 999, "extra": 1}})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto mapping = to_record([](::ryml::ConstNodeRef node)
    -> omni::compat::expected<counted_record,
      std::vector<omni::ryml::issue>> {
    auto count = omni::ryml::core::make_issue(issue_code::out_of_range,
      value_kind::integer, node["count"]);
    auto extra = omni::ryml::core::make_issue(issue_code::unknown_field,
      value_kind::unknown, node["extra"]);
    return omni::compat::unexpected<std::vector<omni::ryml::issue>>{
      {std::move(count), std::move(extra)}};
  });

  omni::ryml::config_t<> options{};
  options.error_policy.error_limit = 1;
  const auto outer = tree->crootref()["outer"];
  const auto result = map_using(options, mapping,
    omni::type_t<counted_record>{}, outer);
  EXPECT_FALSE(result.value);
  EXPECT_EQ(2U, result.diagnostics.issues.size());
  if (result.diagnostics.issues.size() == 2) {
    const auto &count = result.diagnostics.issues[0];
    EXPECT_EQ(issue_code::out_of_range, count.reason);
    EXPECT_EQ(value_kind::integer, count.expected);
    EXPECT_EQ("/outer/count", omni::ryml::detail::render_path(count.path));
    EXPECT_EQ(outer["count"].id(), count.node_id);

    const auto &extra = result.diagnostics.issues[1];
    EXPECT_EQ(issue_code::unknown_field, extra.reason);
    EXPECT_EQ("/outer/extra", omni::ryml::detail::render_path(extra.path));
    EXPECT_EQ(outer["extra"].id(), extra.node_id);
  }
}

struct callback_issue_limit_case {
  std::string name;
  std::uint16_t error_limit;
  omni::compat::optional<std::uint16_t> warning_limit;
  bool warnings;
  std::vector<issue_code> expected;
};

class callback_issue_limits:
    public testing::TestWithParam<callback_issue_limit_case> {};

TEST_P(callback_issue_limits, limits_issues_returned_by_a_callback) {
  const auto &c = GetParam();
  const auto tree = omni::ryml::core::parse("{}");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const bool warnings = c.warnings;
  const auto mapping = to_record([warnings](::ryml::ConstNodeRef node)
    -> omni::compat::expected<counted_record,
      std::vector<omni::ryml::issue>> {
    const auto issue = omni::ryml::core::make_issue(
      issue_code::out_of_range, value_kind::integer, node, warnings);

    return omni::compat::unexpected<std::vector<omni::ryml::issue>>{
      {issue, issue, issue}};
  });

  omni::ryml::config_t<> options{};
  options.error_policy.error_limit = c.error_limit;
  options.error_policy.warning_limit = c.warning_limit;
  const auto result = map_using(options, mapping,
    omni::type_t<counted_record>{}, tree->crootref());

  EXPECT_FALSE(result.value);
  EXPECT_EQ(c.expected.size(), result.diagnostics.issues.size());
  if (c.expected.size() == result.diagnostics.issues.size()) {
    for (std::size_t index = 0; index < c.expected.size(); ++index) {
      SCOPED_TRACE(index);
      EXPECT_EQ(c.expected[index], result.diagnostics.issues[index].reason);
      EXPECT_EQ(c.warnings
          && c.expected[index] != issue_code::warning_limit_exceeded,
        result.diagnostics.issues[index].warning);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(budgets,
  callback_issue_limits,
  testing::Values(
    callback_issue_limit_case{"first_error", 0, {}, false,
      {issue_code::out_of_range}},
    callback_issue_limit_case{"second_error", 1, {}, false,
      {issue_code::out_of_range, issue_code::out_of_range}},
    callback_issue_limit_case{"all_errors", 2, {}, false,
      {issue_code::out_of_range, issue_code::out_of_range,
        issue_code::out_of_range}},
    callback_issue_limit_case{"first_warning", 0, 0, true,
      {issue_code::out_of_range, issue_code::warning_limit_exceeded}},
    callback_issue_limit_case{"second_warning", 0, 1, true,
      {issue_code::out_of_range, issue_code::out_of_range,
        issue_code::warning_limit_exceeded}},
    callback_issue_limit_case{"unlimited_warnings", 0, {}, true,
      {issue_code::out_of_range, issue_code::out_of_range,
        issue_code::out_of_range}}),

  [](const testing::TestParamInfo<callback_issue_limit_case> &c) {
    return c.param.name;
  });

TEST(shape_callback, adapts_a_value_callback_without_adding_assignment) {
  const auto tree = omni::ryml::core::parse(R"({"first": 1})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto mapping = omni::ryml::compose::lift_to_with_diagnostics(
    count_properties);

  const omni::ryml::config_t<> options{};
  const auto result = map_using(options, mapping,
    omni::type_t<counted_record>{}, tree->crootref());

  EXPECT_TRUE(result.value);
  if (result.value)
    EXPECT_EQ(1U, result.value->count);

  EXPECT_TRUE(result.diagnostics.issues.empty());
  EXPECT_FALSE((omni::compat::is_invocable<decltype(mapping),
    omni::ryml::read_state_t<>,
    ::ryml::ConstNodeRef,
    counted_record *>::value));
}

TEST(shape_callback, maps_integer_input_with_a_destination_tag) {
  const auto tree = omni::ryml::core::parse(R"({"number": 42, "text": "42"})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto mapping = omni::ryml::compose::to_integral(
    parse_integer_callback{});

  const omni::ryml::config_t<> options{};
  const auto node = tree->crootref();
  const auto number = map_using(options, mapping,
    omni::type_t<int>{}, node["number"]);
  const auto text = map_using(options, mapping,
    omni::type_t<int>{}, node["text"]);

  EXPECT_EQ(42, number.value);
  EXPECT_TRUE(number.diagnostics.issues.empty());
  EXPECT_FALSE(text.value);
  EXPECT_EQ(1U, text.diagnostics.issues.size());
  for (const auto &entry : text.diagnostics.issues) {
    EXPECT_EQ(issue_code::unexpected_kind, entry.reason);
    EXPECT_EQ(value_kind::integer, entry.expected);
  }
}

TEST(record_callback, maps_into_a_nonassignable_destination) {
  const auto tree = omni::ryml::core::parse(
    R"({"record": {"name": "Ash"}, "array": []})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto map = to_record(map_nonassignable_account{});

  omni::ryml::config_t<> options{};
  options.error_policy.unknown_fields = omni::json::unknown_policy::allow;
  const auto root = tree->crootref();
  nonassignable_account into{7, 0};
  const auto mapped = map_using(options, map, &into, root["record"]);
  EXPECT_TRUE(mapped.value);
  EXPECT_EQ(7, into.id);
  EXPECT_EQ(1U, into.fields);

  const auto rejected = map_using(options, map, &into, root["array"]);
  EXPECT_FALSE(rejected.value);
  EXPECT_EQ(1U, into.fields);
  EXPECT_EQ(1U, rejected.diagnostics.issues.size());
  for (const auto &entry : rejected.diagnostics.issues)
    EXPECT_EQ(issue_code::unexpected_kind, entry.reason);

  const omni::ryml::config_t<> strict{};
  const auto unknown = map_using(strict, map,
    &into, root["record"]);

  EXPECT_FALSE(unknown.value);
  EXPECT_EQ(1U, unknown.diagnostics.issues.size());
  for (const auto &entry : unknown.diagnostics.issues)
    EXPECT_EQ(issue_code::unknown_field, entry.reason);
}

TEST(record_callback, accepts_a_mapping_that_already_returns_diagnostics) {
  const auto tree = omni::ryml::core::parse(R"({"first": 1})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto mapping = to_record(diagnostic_count_callback{});

  const omni::ryml::config_t<> options{};
  const auto mapped = map_using(options, mapping,
    omni::type_t<counted_record>{}, tree->crootref());

  EXPECT_TRUE(mapped.value);
  if (mapped.value)
    EXPECT_EQ(1U, mapped.value->count);

  EXPECT_TRUE(mapped.diagnostics.issues.empty());
}

TEST(record_callback, pointer_only_callback_exposes_only_mutation) {
  struct record {
    std::size_t count;

    record() = delete;
    explicit record(std::size_t count): count(count) {}
  };

  const auto mapping = to_record([](::ryml::ConstNodeRef node, record *into)
    -> omni::compat::expected<omni::compat::monostate, omni::ryml::issue> {
    into->count = node.num_children();
    return omni::compat::monostate{};
  });

  EXPECT_TRUE((omni::compat::is_invocable<decltype(mapping),
    omni::ryml::read_state_t<>,
    ::ryml::ConstNodeRef,
    record *>::value));
  EXPECT_FALSE((omni::compat::is_invocable<decltype(mapping),
    omni::ryml::read_state_t<>,
    ::ryml::ConstNodeRef,
    omni::type_t<record>>::value));
}

// FIXME: [high] Clarify type<T> diagnostics for pointer-only callbacks when T
// cannot be default-constructed; the call produces a template-error chain.
#if 0
TEST(record_callback,
  pointer_only_callback_with_non_default_constructible_value) {
  struct record {
    std::size_t count;

    record() = delete;
    explicit record(std::size_t count): count(count) {}
  };

  const auto tree = omni::ryml::core::parse(R"({"first": 1})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto mapping = to_record([](::ryml::ConstNodeRef node, record *into)
    -> omni::compat::expected<omni::compat::monostate, omni::ryml::issue> {
    into->count = node.num_children();
    return omni::compat::monostate{};
  });

  const omni::ryml::config_t<> options{};
  record into{0};
  const auto assigned = map_using(options, mapping, &into, tree->crootref());
  EXPECT_TRUE(assigned.value);
  EXPECT_EQ(1U, into.count);

  // Creating a value requires a value-producing callback for this record.
  (void)map_using(options, mapping,
    omni::type_t<record>{}, tree->crootref());
}
#endif

struct issue_sentinel { omni::ryml::issue *last; };

constexpr bool operator==(omni::ryml::issue *first, issue_sentinel last) {
  return first == last.last;
}

constexpr bool operator==(issue_sentinel last, omni::ryml::issue *first) {
  return first == last;
}

constexpr bool operator!=(omni::ryml::issue *first, issue_sentinel last) {
  return !(first == last);
}

constexpr bool operator!=(issue_sentinel last, omni::ryml::issue *first) {
  return !(first == last);
}

struct issue_range {
  omni::ryml::issue entries[2];

  omni::ryml::issue *begin() { return entries; }
  issue_sentinel end() { return {entries + 2}; }
};

TEST(shape_callback, accepts_errors_with_a_distinct_sentinel) {
  const auto tree = omni::ryml::core::parse("7");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto mapping = omni::ryml::compose::to_integral(
    [](::ryml::ConstNodeRef node)
      -> omni::compat::expected<int, issue_range> {
      return omni::compat::unexpected<issue_range>{issue_range{{
        omni::ryml::core::make_issue(issue_code::out_of_range,
          value_kind::integer, node),
        omni::ryml::core::make_issue(issue_code::invalid_scalar,
          value_kind::integer, node),
      }}};
    });

  omni::ryml::config_t<> options{};
  options.error_policy.error_limit = 1;
  const auto result = map_using(options, mapping,
    omni::type_t<int>{}, tree->crootref());

  EXPECT_FALSE(result.value);
  EXPECT_EQ(2U, result.diagnostics.issues.size());
  if (result.diagnostics.issues.size() == 2) {
    EXPECT_EQ(issue_code::out_of_range, result.diagnostics.issues[0].reason);
    EXPECT_EQ(issue_code::invalid_scalar, result.diagnostics.issues[1].reason);
  }
}

TEST(shape_callback, requires_state_for_type_tag_callbacks) {
  const auto mapping = omni::ryml::compose::lift_to_with_diagnostics(
    [](::ryml::ConstNodeRef, omni::type_t<int>)
      -> omni::compat::expected<int, omni::ryml::issue> { return 42; });

  EXPECT_FALSE((omni::compat::is_invocable<decltype(mapping),
    omni::ryml::read_state_t<>, ::ryml::ConstNodeRef,
    omni::type_t<int>>::value));
}

#if defined(__cpp_generic_lambdas) && 201304L <= __cpp_generic_lambdas
TEST(shape_callback, accepts_a_generic_state_and_node_callback) {
  const auto tree = omni::ryml::core::parse("42");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto mapping = omni::ryml::compose::lift_to_with_diagnostics(
    [](auto state, auto node) {
      return omni::ryml::core::map_node_to_integral(
        std::move(state), node, omni::type_t<int>{});
    });

  const omni::ryml::config_t<> options{};
  const auto result = map_using(options, mapping,
    omni::type_t<int>{}, tree->crootref());

  EXPECT_EQ(42, result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
}

TEST(shape_callback, assigns_from_a_generic_state_and_explicit_node_callback) {
  const auto tree = omni::ryml::core::parse("42");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto mapping = omni::ryml::compose::to_integral(
    [](auto state, ::ryml::ConstNodeRef node) {
      return omni::ryml::core::map_node_to_integral(
        std::move(state), node, omni::type_t<int>{});
    });

  const omni::ryml::config_t<> options{};
  int into{};
  const auto result = map_using(options, mapping,
    &into, tree->crootref());

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  EXPECT_EQ(42, into);
}
#endif

} // namespace

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
