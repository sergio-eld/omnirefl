#include "json_match.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#if 201703L <= OMNI_CPLUSPLUS && defined(__has_include)
#  if __has_include(<string_view>)
#    include <string_view>
#  endif
#endif

namespace {

// Forward elements into storage: initializer_list would copy const elements.
template <typename Value = void, typename First, typename... Args>
auto make_vector(First &&first, Args &&...args)
  -> std::vector<omni::compat::conditional_t<std::is_void<Value>::value,
    omni::compat::decay_t<First>,
    Value>> {
  using element_type = omni::compat::conditional_t<std::is_void<Value>::value,
    omni::compat::decay_t<First>,
    Value>;

  std::vector<element_type> values;
  values.reserve(1 + sizeof...(Args));
  values.emplace_back(std::forward<First>(first));

  // C++11 pack expansion; list initialization preserves insertion order.
  const int inserted[]{0,
    (values.emplace_back(std::forward<Args>(args)), 0)...};
  (void)inserted;
  return values;
}

// Keep this translation unit free of reflected records: these roots must not
// depend on metadata discovered through unrelated record serialization calls.
TEST(serialization_scalar_roots, writes_scalar_values_without_record_metadata) {
  EXPECT_EQ("42", omni::ryml::as_json(42));
  EXPECT_EQ("true", omni::ryml::as_json(true));
  EXPECT_EQ("1.5", omni::ryml::as_json(1.5));
  EXPECT_EQ(R"("text")", omni::ryml::as_json(std::string{"text"}));
  EXPECT_EQ(R"("")", omni::ryml::as_json(std::string{}));
}

TEST(serialization_scalar_roots, writes_string_array_without_record_metadata) {
  EXPECT_TRUE(serialization_test::matches_json(
    R"(["first", "second"])",
    omni::ryml::as_json(std::vector<std::string>{"first", "second"})));
}

TEST(serialization_scalar_roots, writes_boolean_array_without_record_metadata) {
  EXPECT_TRUE(serialization_test::matches_json(
    R"([true, false])", omni::ryml::as_json(std::vector<bool>{true, false})));
}

TEST(serialization_scalar_roots, writes_empty_array_without_record_metadata) {
  EXPECT_TRUE(serialization_test::matches_json(
    R"([])", omni::ryml::as_json(std::vector<int>{})));
}

struct alternate_char_traits: std::char_traits<char> {};

TEST(serialization_scalar_roots, writes_basic_string_with_custom_traits) {
  const std::basic_string<char, alternate_char_traits> text{"12"};
  EXPECT_EQ(R"("12")", omni::ryml::as_json(text));
}

TEST(serialization_scalar_roots, writes_c4_string_views) {
  const std::string storage{"prefix true suffix"};
  const c4::csubstr view{storage.data() + 7, 4};

  EXPECT_EQ(R"("true")", omni::ryml::as_json(view));
  EXPECT_EQ(R"("")", omni::ryml::as_json(c4::csubstr{}));
}

#if defined(__cpp_lib_string_view)
TEST(serialization_scalar_roots, writes_standard_string_views) {
  const std::string storage{"prefix null suffix"};
  const std::string_view view{storage.data() + 7, 4};

  EXPECT_EQ(R"("null")", omni::ryml::as_json(view));
  EXPECT_EQ(R"("")", omni::ryml::as_json(std::string_view{}));
}

TEST(serialization_scalar_roots, writes_standard_string_view_array) {
  EXPECT_TRUE(serialization_test::matches_json(
    R"(["first", "second"])",
    omni::ryml::as_json(std::vector<std::string_view>{"first", "second"})));
}
#endif

TEST(deserialization_scalar_roots, reads_scalars_without_record_metadata) {
  const auto integer = serialization_test::deserialize(omni::type_t<int>{}, "42");
  EXPECT_TRUE(integer.value);
  if (integer.value) {
    EXPECT_EQ(42, *integer.value);
  }
  EXPECT_TRUE(integer.diagnostics.issues.empty());

  const auto boolean = serialization_test::deserialize(omni::type_t<bool>{}, "true");
  EXPECT_TRUE(boolean.value);
  if (boolean.value) {
    EXPECT_TRUE(*boolean.value);
  }

  const auto real = serialization_test::deserialize(omni::type_t<double>{}, "1.5");
  EXPECT_TRUE(real.value);
  if (real.value) {
    EXPECT_EQ(1.5, *real.value);
  }

  const auto text = serialization_test::deserialize(
    omni::type_t<std::string>{}, R"("text")");

  EXPECT_TRUE(text.value);
  if (text.value) {
    EXPECT_EQ("text", *text.value);
  }
}

TEST(deserialization_scalar_roots, keeps_a_valid_false_value) {
  const auto result = serialization_test::deserialize(omni::type_t<bool>{}, "false");
  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_FALSE(*result.value);
  }
}

struct integer_range_case {
  std::string name;
  std::string source;
  omni::compat::expected<std::int8_t, omni::json::issue_code> expected;
};

class scalar_integer_ranges:
    public testing::TestWithParam<integer_range_case> {};

TEST_P(scalar_integer_ranges, returns_a_value_or_a_range_error) {
  const auto &c = GetParam();
  const auto result = serialization_test::deserialize(
    omni::type_t<std::int8_t>{}, c.source);
  EXPECT_EQ(bool(c.expected), bool(result.value));
  if (c.expected && result.value) {
    EXPECT_EQ(*c.expected, *result.value);
    EXPECT_TRUE(result.diagnostics.issues.empty());
  }
  if (!c.expected) {
    EXPECT_EQ(1U, result.diagnostics.issues.size());
    if (1U == result.diagnostics.issues.size()) {
      EXPECT_EQ(c.expected.error(), result.diagnostics.issues.front().reason);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(boundaries,
  scalar_integer_ranges,
  testing::ValuesIn(std::vector<integer_range_case>{
    {"zero", "0", std::int8_t{0}},
    {
      "minimum",
      std::to_string(std::numeric_limits<std::int8_t>::min()),
      std::numeric_limits<std::int8_t>::min(),
    },
    {
      "maximum",
      std::to_string(std::numeric_limits<std::int8_t>::max()),
      std::numeric_limits<std::int8_t>::max(),
    },
    {
      "below_minimum",
      std::to_string(std::numeric_limits<std::int8_t>::min() - 1),
      omni::compat::unexpected<omni::json::issue_code>{
        omni::json::issue_code::out_of_range,
      },
    },
    {
      "above_maximum",
      std::to_string(std::numeric_limits<std::int8_t>::max() + 1),
      omni::compat::unexpected<omni::json::issue_code>{
        omni::json::issue_code::out_of_range,
      },
    },
  }),
  [](const testing::TestParamInfo<integer_range_case> &c) {
    return c.param.name;
  });

TEST(deserialization_scalar_roots, reports_invalid_root_values) {
  const auto result = serialization_test::deserialize(omni::type_t<int>{}, R"("bad")");
  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  for (const auto &issue : result.diagnostics.issues) {
    EXPECT_EQ(omni::json::issue_code::invalid_scalar, issue.reason);
    EXPECT_EQ(omni::json::value_kind::integer, issue.expected);
    EXPECT_TRUE(issue.path.empty());
  }
}

TEST(deserialization_scalar_roots, reads_scalar_sequence_roots) {
  const auto numbers = serialization_test::deserialize(
    omni::type_t<std::vector<int>>{}, "[7, 42]");

  const std::vector<int> expected_numbers{7, 42};
  EXPECT_TRUE(numbers.value);
  if (numbers.value) {
    EXPECT_EQ(expected_numbers, *numbers.value);
  }

  const auto booleans = serialization_test::deserialize(
    omni::type_t<std::vector<bool>>{}, "[true, false]");

  const std::vector<bool> expected_booleans{true, false};
  EXPECT_TRUE(booleans.value);
  if (booleans.value) {
    EXPECT_EQ(expected_booleans, *booleans.value);
  }

  const auto nested = serialization_test::deserialize(
    omni::type_t<std::vector<std::vector<int>>>{}, "[[7], [], [42]]");

  const std::vector<std::vector<int>> expected_nested{{7}, {}, {42}};
  EXPECT_TRUE(nested.value);
  if (nested.value) {
    EXPECT_EQ(expected_nested, *nested.value);
  }
}

struct invalid_input_case {
  std::string name;
  std::string source;
};

class invalid_deserialization_input:
    public testing::TestWithParam<invalid_input_case> {};

TEST_P(invalid_deserialization_input, reports_failure_with_diagnostics) {
  const auto result = serialization_test::deserialize(
    omni::type_t<int>{}, GetParam().source);
  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (1U == result.diagnostics.issues.size()) {
    EXPECT_FALSE(result.diagnostics.issues.front().warning);
    EXPECT_TRUE(result.diagnostics.issues.front().path.empty());
  }
  EXPECT_FALSE(serialization_test::render_diagnostics(result.diagnostics).empty());
}

INSTANTIATE_TEST_SUITE_P(documents,
  invalid_deserialization_input,
  testing::ValuesIn(std::vector<invalid_input_case>{
    {/*name=*/"empty", /*source=*/""},
    {/*name=*/"whitespace", /*source=*/" \n\t"},
    {/*name=*/"unterminated_json", /*source=*/"["},
    {/*name=*/"unterminated_yaml", /*source=*/"value: ["},
    {/*name=*/"wrong_value_kind", /*source=*/"[]"},
    {/*name=*/"null_scalar", /*source=*/"null"},
  }),
  [](const testing::TestParamInfo<invalid_input_case> &c) {
    return c.param.name;
  });

TEST(deserialization_scalar_roots, reports_sequence_element_paths) {
  const auto load = serialization_test::deserialize.tdm_config(
    [](serialization_test::config s) {
      s.error_policy.error_limit = 1;
      return s;
    });

  const auto result = load(
    omni::type_t<std::vector<int>>{}, R"(["bad", "also bad", 42])");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(2U, result.diagnostics.issues.size());
  EXPECT_EQ("/0: \"bad\" is not an integer\n/1: \"also bad\" is not an integer",
    serialization_test::render_diagnostics(result.diagnostics));
}

class optional_serialization:
    public testing::TestWithParam<::ryml::EmitType_e> {};

TEST_P(optional_serialization, writes_present_and_empty_scalar_roots) {
  const omni::ryml::serialize_t emit{GetParam()};
  EXPECT_TRUE(serialization_test::matches_json(
    "null", emit(omni::compat::optional<int>{})));
  EXPECT_TRUE(serialization_test::matches_json(
    "0", emit(omni::compat::optional<int>{0})));
  EXPECT_TRUE(serialization_test::matches_json(
    "false", emit(omni::compat::optional<bool>{false})));
  EXPECT_TRUE(serialization_test::matches_json(
    R"("null")", emit(omni::compat::optional<std::string>{"null"})));
}

TEST_P(optional_serialization, preserves_present_false_values) {
  const omni::ryml::serialize_t emit{GetParam()};
  const std::vector<omni::compat::optional<int>> integers{
    omni::compat::nullopt, 0, 7, omni::compat::nullopt};

  const std::vector<omni::compat::optional<bool>> booleans{
    false, omni::compat::nullopt, true};

  const std::vector<omni::compat::optional<std::string>> strings{
    std::string{}, omni::compat::nullopt, std::string{"null"}};

  EXPECT_TRUE(serialization_test::matches_json("[0, 7]", emit(integers)));
  EXPECT_TRUE(serialization_test::matches_json(
    "[false, true]", emit(booleans)));
  EXPECT_TRUE(serialization_test::matches_json(
    R"(["", "null"])", emit(strings)));
}

TEST_P(optional_serialization, writes_empty_sequence_for_empty_elements) {
  const std::vector<omni::compat::optional<int>> values{
    omni::compat::nullopt, omni::compat::nullopt};

  EXPECT_TRUE(serialization_test::matches_json(
    "[]", omni::ryml::serialize_t{GetParam()}(values)));
}

TEST_P(optional_serialization, handles_nested_optionals_and_sequences) {
  const omni::ryml::serialize_t emit{GetParam()};
  const auto empty = omni::compat::make_optional<omni::compat::optional<int>>(
    omni::compat::nullopt);

  const std::vector<omni::compat::optional<omni::compat::optional<int>>> nested{
    empty, omni::compat::make_optional<omni::compat::optional<int>>(7)};

  const auto sequences = make_vector<omni::compat::optional<std::vector<int>>>(
    omni::compat::nullopt, std::vector<int>{}, std::vector<int>{7});

  EXPECT_TRUE(serialization_test::matches_json("null", emit(empty)));
  EXPECT_TRUE(serialization_test::matches_json("[7]", emit(nested)));
  EXPECT_TRUE(serialization_test::matches_json("[[], [7]]", emit(sequences)));
}

INSTANTIATE_TEST_SUITE_P(formats,
  optional_serialization,
  testing::Values(::ryml::EMIT_JSON, ::ryml::EMIT_YAML));

TEST(serialization_trees, builds_scalar_and_optional_sequence_roots) {
  const auto scalar = omni::ryml::core::as_owning_tree(42);
  EXPECT_TRUE(serialization_test::matches_node("42", scalar.crootref()));

  const std::vector<omni::compat::optional<int>> values{
    omni::compat::nullopt, 0, 7};

  const auto sequence = omni::ryml::core::as_owning_tree(values);
  EXPECT_TRUE(serialization_test::matches_node("[0, 7]", sequence.crootref()));

  const auto empty =
    omni::ryml::core::as_owning_tree(omni::compat::optional<int>{});
  EXPECT_TRUE(serialization_test::matches_node("null", empty.crootref()));
}

TEST(serialization_trees, copies_borrowed_string_data_into_the_arena) {
  std::string source{"original text"};
  const auto tree = omni::ryml::core::as_owning_tree(
    c4::csubstr{source.data(), source.size()});

  source.front() = 'x';
  EXPECT_TRUE(serialization_test::matches_node(
    R"("original text")", tree.crootref()));
}

template <typename Value, typename = void>
struct can_make_ref_tree: std::false_type {};

template <typename Value>
struct can_make_ref_tree<Value,
  omni::compat::void_t<decltype(omni::ryml::core::as_ref_tree(
    std::declval<Value>()))>>: std::true_type {};

TEST(serialization_trees, borrows_string_storage_from_const_lvalues) {
  const std::string source{"borrowed text"};
  const auto tree = omni::ryml::core::as_ref_tree(source);
  EXPECT_EQ(static_cast<const void *>(source.data()),
    static_cast<const void *>(tree.crootref().val().str));
  EXPECT_TRUE(serialization_test::matches_node(
    R"("borrowed text")", tree.crootref()));

  static_assert(can_make_ref_tree<std::string &>::value, "mutable lvalue");
  static_assert(can_make_ref_tree<const std::string &>::value, "const lvalue");
  static_assert(!can_make_ref_tree<std::string &&>::value, "reject temporary");
  static_assert(!can_make_ref_tree<const std::string &&>::value,
    "reject const temporary");
}

TEST(serialization_trees, borrows_optional_sequence_strings_and_owns_numbers) {
  const std::vector<omni::compat::optional<std::string>> source{
    std::string{"first"}, omni::compat::nullopt, std::string{"last"}};

  const auto tree = omni::ryml::core::as_ref_tree(source);
  EXPECT_TRUE(serialization_test::matches_node(
    R"(["first", "last"])", tree.crootref()));
  EXPECT_EQ(static_cast<const void *>(source.front()->data()),
    static_cast<const void *>(tree.crootref()[0].val().str));
  EXPECT_EQ(static_cast<const void *>(source.back()->data()),
    static_cast<const void *>(tree.crootref()[1].val().str));

  const int number = 42;
  const auto scalar = omni::ryml::core::as_ref_tree(number);
  EXPECT_TRUE(scalar.in_arena(scalar.crootref().val()));
  EXPECT_TRUE(serialization_test::matches_node("42", scalar.crootref()));

  // TODO: [medium] Guard sequence child access after the nonfatal shape check
  // so malformed output reports a failure instead of aborting the suite.
}

} // namespace
