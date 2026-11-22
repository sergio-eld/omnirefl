#include "json_match.hpp"

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(__GNUC__) && !defined(__clang__)
// GCC warns about GTest conditionals inside unbraced assertion guards.
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdangling-else"
#endif

namespace {

// Each route has its own record specialization, so another route cannot
// supply the metadata it needs.
struct vector_map {};
struct map_vector {};
struct optional_map {};
struct map_optional {};
struct custom_key {};
struct empty_maps {};
struct invalid_map {};

template <typename Route>
struct record {
  int value;

  bool operator==(const record &other) const {
    return this->value == other.value;
  }
};

struct my_string {
  std::string text;

  bool operator<(const my_string &other) const {
    return this->text < other.text;
  }

  bool operator==(const my_string &other) const {
    return this->text == other.text;
  }
};

struct immutable_number {
  const int value;

  immutable_number() = delete;
  explicit immutable_number(int value): value(value) {}
};

struct default_state_number {
  int value;
};

namespace custom_depth {

struct leaf { int checked; int raw; };
struct node { leaf data; std::vector<node> children; };

struct read_leaf {
  OMNI_CONSTRAINED_TEMPLATE((typename State),
    (omni::ryml::is_read_state<State>::value))
  auto operator()(State state, ::ryml::ConstNodeRef node,
    omni::type_t<leaf>) const -> omni::ryml::with_diagnostics_t<leaf> {
    return omni::ryml::map_integer_node(
      state.child(OMNI_META_STRING("checked")), node["checked"],
      omni::type_t<int>{}).and_then([&state, &node](int checked)
        -> omni::ryml::with_diagnostics_t<leaf> {
      return omni::ryml::core::map_node_to_integral(
        state.child(OMNI_META_STRING("raw")), node["raw"],
        omni::type_t<int>{}).transform([checked](int raw) -> leaf {
          return {checked, raw};
        });
    });
  }
};

} // namespace custom_depth

namespace diagnostic_callbacks {

std::string classification(const omni::ryml::diagnostics &report) {
  if (report.issues.empty())
    return "success";

  return omni::json::issue_code::parse_error == report.issues.front().reason
    ? "syntax_error" : "mapping_error";
}

struct renderer {
  std::string operator()(const ::ryml::Tree &,
    const omni::ryml::diagnostics &report) const {
    return "configured/" + classification(report);
  }
};

// An unrelated application helper must not intercept deserialization errors.
inline std::string parse_error_diagnostics(
  const omni::ryml::config_t<renderer> &, std::string) {
  return "application helper";
}

struct const_category_renderer {
  std::string operator()(const ::ryml::Tree &,
    const omni::ryml::diagnostics &report) const & {
    return "const_lvalue/" + classification(report);
  }

  std::string operator()(const ::ryml::Tree &,
    const omni::ryml::diagnostics &report) const && {
    return "const_rvalue/" + classification(report);
  }
};

struct consuming_renderer {
  std::unique_ptr<std::string> prefix;

  std::string operator()(const ::ryml::Tree &,
    const omni::ryml::diagnostics &report) && {
    return *this->prefix + classification(report);
  }
};

struct const_rvalue_only_renderer {
  std::string operator()(const ::ryml::Tree &,
    const omni::ryml::diagnostics &report) const && {
    return "const_rvalue/" + classification(report);
  }
};

} // namespace diagnostic_callbacks

} // namespace

#if defined(OMNI_TOOL_RUN)
OMNI_SKIP_METADATA(my_string);
#endif

template <>
struct omni::traits::cp::as_string<my_string> {
  omni::compat::string_view operator()(const my_string &value) const {
    return {value.text.data(), value.text.size()};
  }
};

template <>
struct omni::traits::cp::from_string<my_string> {
  my_string operator()(omni::compat::string_view text) const {
    return {omni::adapt::from_string(omni::type_t<std::string>{}, text)};
  }
};

template <>
struct omni::ryml::cp::to_type_t<immutable_number> {
  template <typename State>
  auto operator()(State state,
    ::ryml::ConstNodeRef node,
    omni::type_t<immutable_number>) const
    -> omni::ryml::with_diagnostics_t<immutable_number> {
    return omni::ryml::map_integer_node(std::move(state),
      node,
      omni::type_t<int>{})
      .transform([](int value) -> immutable_number {
        return immutable_number{value};
      });
  }
};

template <>
struct omni::ryml::cp::to_type_t<default_state_number> {
  OMNI_CONSTRAINED_TEMPLATE((typename State),
    (omni::ryml::is_read_state<State>::value))
  auto operator()(State state,
    ::ryml::ConstNodeRef node,
    omni::type_t<default_state_number>) const
    -> omni::ryml::with_diagnostics_t<default_state_number> {
    return omni::ryml::map_integer_node(std::move(state),
      node,
      omni::type_t<int>{})
      .transform([](int value) -> default_state_number { return {value}; });
  }
};

template <>
struct omni::ryml::cp::to_type_t<unsigned short> {
  template <typename State>
  auto operator()(State state,
    ::ryml::ConstNodeRef node,
    unsigned short *into) const
    -> omni::ryml::with_diagnostics_t<omni::compat::monostate> {
    return omni::ryml::map_integer_node(std::move(state),
      node.is_seq() && 1 == node.num_children() ? node[0] : node,
      into);
  }
};

// This string's custom reader also accepts a one-element scalar wrapper.
// That observable distinction checks that registration overrides the default.
// C++14 supports callable variable-template registration and generic lambdas.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <>
const auto omni::ryml::cp::to_type<my_string> =
  [](auto state, ::ryml::ConstNodeRef node, my_string *into) {
    return omni::ryml::map_string_node(std::move(state),
      node.is_seq() && 1 == node.num_children() ? node[0] : node,
      into);
  };
#else
template <>
struct omni::ryml::cp::to_type_t<my_string> {
  template <typename State>
  auto operator()(State state, ::ryml::ConstNodeRef node, my_string *into) const
    -> omni::ryml::with_diagnostics_t<omni::compat::monostate> {
    return omni::ryml::map_string_node(std::move(state),
      node.is_seq() && 1 == node.num_children() ? node[0] : node,
      into);
  }
};
#endif

template <>
struct omni::ryml::cp::to_type_t<custom_depth::leaf>:
    decltype(omni::ryml::compose::map_object_node(
      custom_depth::read_leaf{})) {};

namespace {

TEST(deserialize_diagnostics, accepts_const_rvalue_only_callbacks) {
  const auto load = omni::ryml::deserialize.tdm_config(
    [](omni::ryml::config_t<> options) {
      return options.tdm_transform_diagnostics(
        [](omni::ryml::render_diagnostics_t) {
          return diagnostic_callbacks::const_rvalue_only_renderer{};
        });
    });

  EXPECT_FALSE((omni::compat::is_invocable<decltype(load) &,
    omni::type_t<int>, std::string>::value));
  EXPECT_FALSE((omni::compat::is_invocable<decltype(load) &,
    int *, std::string>::value));

  const auto value = std::move(load)(omni::type_t<int>{}, "42");
  EXPECT_EQ(42, value.value);
  EXPECT_EQ("const_rvalue/success", value.diagnostics);

  int into = 7;
  const auto result = std::move(load)(&into, "42");
  EXPECT_TRUE(result.value);
  EXPECT_EQ(42, into);
  EXPECT_EQ("const_rvalue/success", result.diagnostics);
}

struct diagnostics_case {
  std::string name;
  std::string document;
  omni::compat::optional<int> expected;
};

class deserialize_diagnostics:
    public testing::TestWithParam<diagnostics_case> {};

TEST_P(deserialize_diagnostics, uses_the_configured_value_renderer) {
  const auto &c = GetParam();
  const auto load = omni::ryml::deserialize.tdm_config(
    [](omni::ryml::config_t<> options) {
      return options.tdm_transform_diagnostics(
        [](omni::ryml::render_diagnostics_t) {
          return diagnostic_callbacks::renderer{};
        });
    });

  const auto result = load(omni::type_t<int>{}, c.document);

  EXPECT_EQ(c.expected, result.value);
  EXPECT_EQ("configured/" + c.name, result.diagnostics);
}

TEST_P(deserialize_diagnostics, uses_the_configured_mutating_renderer) {
  const auto &c = GetParam();
  const auto load = omni::ryml::deserialize.tdm_config(
    [](omni::ryml::config_t<> options) {
      return options.tdm_transform_diagnostics(
        [](omni::ryml::render_diagnostics_t) {
          return diagnostic_callbacks::renderer{};
        });
    });

  int into = 7;
  const auto result = load(&into, c.document);

  EXPECT_EQ(bool(c.expected), bool(result.value));
  EXPECT_EQ("configured/" + c.name, result.diagnostics);
  if (c.expected)
    EXPECT_EQ(*c.expected, into);
  else if ("syntax_error" == c.name)
    EXPECT_EQ(7, into);
}

TEST_P(deserialize_diagnostics, preserves_const_rvalues_for_values) {
  const auto &c = GetParam();
  const auto load = omni::ryml::deserialize.tdm_config(
    [](omni::ryml::config_t<> options) {
      return options.tdm_transform_diagnostics(
        [](omni::ryml::render_diagnostics_t) {
          return diagnostic_callbacks::const_category_renderer{};
        });
    });

  const auto lvalue = load(omni::type_t<int>{}, c.document);
  const auto rvalue = std::move(load)(omni::type_t<int>{}, c.document);

  EXPECT_EQ(c.expected, lvalue.value);
  EXPECT_EQ("const_lvalue/" + c.name, lvalue.diagnostics);
  EXPECT_EQ(c.expected, rvalue.value);
  EXPECT_EQ("const_rvalue/" + c.name, rvalue.diagnostics);
}

TEST_P(deserialize_diagnostics, preserves_const_rvalues_for_mutation) {
  const auto &c = GetParam();
  const auto load = omni::ryml::deserialize.tdm_config(
    [](omni::ryml::config_t<> options) {
      return options.tdm_transform_diagnostics(
        [](omni::ryml::render_diagnostics_t) {
          return diagnostic_callbacks::const_category_renderer{};
        });
    });

  int into = 7;
  const auto result = std::move(load)(&into, c.document);

  EXPECT_EQ(bool(c.expected), bool(result.value));
  EXPECT_EQ("const_rvalue/" + c.name, result.diagnostics);
  if (c.expected)
    EXPECT_EQ(*c.expected, into);
  else if ("syntax_error" == c.name)
    EXPECT_EQ(7, into);
}

TEST_P(deserialize_diagnostics, consumes_configuration_for_values) {
  const auto &c = GetParam();
  auto load = omni::ryml::deserialize.tdm_config(
    [](omni::ryml::config_t<> options) {
      return options.tdm_transform_diagnostics(
        [](omni::ryml::render_diagnostics_t) {
          return diagnostic_callbacks::consuming_renderer{
            omni::compat::make_unique<std::string>("consumed/"),
          };
        });
    });

  const auto result = std::move(load)(omni::type_t<int>{}, c.document);

  EXPECT_EQ(c.expected, result.value);
  EXPECT_EQ("consumed/" + c.name, result.diagnostics);
}

TEST_P(deserialize_diagnostics, consumes_configuration_for_mutation) {
  const auto &c = GetParam();
  auto load = omni::ryml::deserialize.tdm_config(
    [](omni::ryml::config_t<> options) {
      return options.tdm_transform_diagnostics(
        [](omni::ryml::render_diagnostics_t) {
          return diagnostic_callbacks::consuming_renderer{
            omni::compat::make_unique<std::string>("consumed/"),
          };
        });
    });

  int into = 7;
  const auto result = std::move(load)(&into, c.document);

  EXPECT_EQ(bool(c.expected), bool(result.value));
  EXPECT_EQ("consumed/" + c.name, result.diagnostics);
  if (c.expected)
    EXPECT_EQ(*c.expected, into);
  else if ("syntax_error" == c.name)
    EXPECT_EQ(7, into);
}

INSTANTIATE_TEST_SUITE_P(documents,
  deserialize_diagnostics,
  testing::ValuesIn(std::vector<diagnostics_case>{
    {"success", "42", 42},
    {"mapping_error", R"("bad")", omni::compat::nullopt},
    {"syntax_error", "[", omni::compat::nullopt},
  }),

  [](const testing::TestParamInfo<diagnostics_case> &info) {
    return info.param.name;
  });

TEST(mapping_dispatch, bound_config_passes_a_read_state) {
  const auto result = omni::ryml::deserialize(
    omni::type_t<default_state_number>{}, "42");

  EXPECT_TRUE(result.value);
  if (result.value)
    EXPECT_EQ(42, result.value->value);

  EXPECT_TRUE(result.diagnostics.empty());
}

TEST(tree_serialization, exposes_callable_tree_builders) {
  const auto own = omni::ryml::as_owning_tree;
  const auto borrow = omni::ryml::as_ref_tree;
  const std::string source = "sensor";
  const auto owned = own(std::string{"sensor"});
  const auto borrowed = borrow(source);

  EXPECT_TRUE(serialization_test::matches_node(
    R"("sensor")", owned.crootref()));

  EXPECT_TRUE(serialization_test::matches_node(
    R"("sensor")", borrowed.crootref()));

  EXPECT_FALSE((omni::compat::is_invocable<
    omni::ryml::as_ref_tree_t<>, std::string &&>::value));
}

TEST(mapping_dispatch, constructs_an_immutable_value_from_a_node) {
  const auto parsed = omni::ryml::core::parse("42");

  EXPECT_TRUE(parsed);
  if (parsed) {
    const auto result = omni::ryml::map_by_schema(
      omni::type_t<immutable_number>{}, *parsed);

    EXPECT_TRUE(result.value);
    if (result.value)
      EXPECT_EQ(42, result.value->value);

    EXPECT_TRUE(result.diagnostics.issues.empty());
  }
}

TEST(mapping_dispatch, deserializes_an_immutable_value) {
  const auto result = serialization_test::deserialize(
    omni::type_t<immutable_number>{}, "42");

  EXPECT_TRUE(result.value);
  if (result.value)
    EXPECT_EQ(42, result.value->value);

  EXPECT_TRUE(result.diagnostics.issues.empty());
}

TEST(mapping_dispatch, fundamental_registration_applies_at_the_root) {
  const auto result = serialization_test::deserialize(
    omni::type_t<unsigned short>{}, "[7]");

  EXPECT_EQ(7, result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
}

TEST(mapping_dispatch, fundamental_registration_applies_inside_sequences) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::vector<unsigned short>>{}, "[[7], [42]]");
  const std::vector<unsigned short> expected{7, 42};

  EXPECT_EQ(expected, result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
}

TEST(mapping_dispatch, vector_of_maps_discovers_only_the_mapped_record) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::vector<std::map<std::string, record<vector_map>>>>{},
    R"([{"4": {"value": 8}, "15": {"value": 16}}, {}, {"23": {"value": 42}}])");

  const std::vector<std::map<std::string, record<vector_map>>> expected{
    {{"4", {8}}, {"15", {16}}},
    {},
    {{"23", {42}}},
  };

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_EQ(expected, *result.value);
}

TEST(mapping_dispatch, map_of_vectors_discovers_the_record) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::map<std::string, std::vector<record<map_vector>>>>{},
    R"({"west": [{"value": 4}, {"value": 8}], "empty": []})");

  const std::map<std::string, std::vector<record<map_vector>>> expected{
    {"west", {{4}, {8}}},
    {"empty", {}},
  };

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_EQ(expected, *result.value);
}

TEST(mapping_dispatch, optional_sequence_of_maps_discovers_the_record) {
  const auto result = serialization_test::deserialize(
    omni::type_t<omni::compat::optional<std::vector<std::map<std::string,
      record<optional_map>>>>>{},

    R"([{}, {"first": {"value": 15}}])");

  const std::vector<std::map<std::string, record<optional_map>>> expected{
    {},
    {{"first", {15}}},
  };

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_TRUE(*result.value);
    if (*result.value)
      EXPECT_EQ(expected, **result.value);
  }
}

TEST(mapping_dispatch, maps_of_optional_records_preserve_null_values) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::vector<std::map<std::string,
      omni::compat::optional<record<map_optional>>>>>{},

    R"([{"present": {"value": 23}, "absent": null}])");

  const std::vector<std::map<std::string,
    omni::compat::optional<record<map_optional>>>> expected{
    {
      {"present", record<map_optional>{23}},
      {"absent", omni::compat::nullopt},
    },
  };

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_EQ(expected, *result.value);
}

TEST(mapping_dispatch, empty_compounds_still_discover_the_record_type) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::vector<std::map<std::string, record<empty_maps>>>>{},
    "[]");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_TRUE(result.value->empty());
}

TEST(mapping_dispatch, nested_errors_retain_the_document_path) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::vector<std::map<std::string, record<invalid_map>>>>{},
    R"([{"west": {"value": "bad"}}])");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  for (const auto &entry : result.diagnostics.issues) {
    EXPECT_EQ(omni::json::issue_code::unexpected_kind, entry.reason);
    EXPECT_EQ("/0/west/value", omni::ryml::detail::render_path(entry.path));
  }
}

TEST(mapping_dispatch, disabled_string_registration_needs_no_reflected_scope) {
  const auto result = serialization_test::deserialize(
    omni::type_t<my_string>{}, R"(["hello"])");
  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_EQ("hello", result.value->text);
}

TEST(mapping_dispatch, disabled_strings_inside_sequences_need_no_reflection) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::vector<my_string>>{}, R"([["first"], ["second"]])");
  const std::vector<my_string> expected{{"first"}, {"second"}};

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_EQ(expected, *result.value);
}

TEST(mapping_dispatch, disabled_string_keys_do_not_hide_the_mapped_record) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::vector<std::map<my_string, record<custom_key>>>>{},
    R"([{"west": {"value": 42}}, {}])");

  const std::vector<std::map<my_string, record<custom_key>>> expected{
    {{{"west"}, {42}}},
    {},
  };

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_EQ(expected, *result.value);
}

namespace local_mapping {

struct record { int value; };

struct add_integer {
  int increment;

  template <typename State>
  auto operator()(State state, ::ryml::ConstNodeRef node,
    omni::type_t<int> type) const -> omni::ryml::with_diagnostics_t<int> {
    return omni::ryml::map_integer_node(std::move(state), node, type)
      .transform([this](int value) { return value + this->increment; });
  }
};

struct select_mapping {
  int increment;

  template <typename State, typename T>
  auto operator()(const State &state, omni::type_t<T> type) const
    -> decltype(omni::ryml::reflected_scope::fn::mapping_to_type(state, type)) {
    return omni::ryml::reflected_scope::fn::mapping_to_type(state, type);
  }

  template <typename State>
  add_integer operator()(State state, omni::type_t<int>) const {
    constexpr bool selected = state.path().equals("root.value");
    return {selected ? this->increment : 0};
  }
};

} // namespace local_mapping

namespace unchecked_nodes {

enum class status { ready };

struct record {
  bool enabled;
  short count;
  double weight;
  std::string label;
  std::vector<int> values;
  std::map<std::string, int> scores;
  omni::compat::optional<int> extra;
  status state;
};

struct select_mapping {
  template <typename State, typename T>
  auto operator()(State, omni::type_t<T> type) const
    -> decltype(omni::ryml::reflected_scope::mapping_to_type(type)
      .tdm_with_node_check(omni::fn::default_value<std::false_type>{})) {
    return omni::ryml::reflected_scope::mapping_to_type(type)
      .tdm_with_node_check(omni::fn::default_value<std::false_type>{});
  }

  template <typename State>
  auto operator()(State, omni::type_t<short>) const
    -> decltype(omni::ryml::map_arithmetic_node.tdm_with_node_check(
      omni::fn::default_value<std::false_type>{})) {
    return omni::ryml::map_arithmetic_node.tdm_with_node_check(
      omni::fn::default_value<std::false_type>{});
  }
};

} // namespace unchecked_nodes

TEST(mapping_dispatch, built_in_readers_allow_prevalidated_nodes) {
  const auto deserialize = serialization_test::deserialize
    .tdm_map_node(omni::fn::default_value<unchecked_nodes::select_mapping>{});
  const auto result = deserialize(omni::type_t<unchecked_nodes::record>{},
    R"({"enabled": true, "count": 3, "weight": 2.5, "label": "ready",
      "values": [4, 8], "scores": {"math": 15},
      "extra": 16, "state": "ready"})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_TRUE(result.value->enabled);
    EXPECT_EQ(3, result.value->count);
    EXPECT_EQ(2.5, result.value->weight);
    EXPECT_EQ("ready", result.value->label);
    EXPECT_EQ((std::vector<int>{4, 8}), result.value->values);
    EXPECT_EQ((std::map<std::string, int>{{"math", 15}}), result.value->scores);
    EXPECT_EQ(16, result.value->extra);
    EXPECT_EQ(unchecked_nodes::status::ready, result.value->state);
  }
}

TEST(mapping_dispatch, local_map_node_customization) {
  const auto customize = [](omni::ryml::reflected_scope::mapping_to_type_t) {
    return local_mapping::select_mapping{5};
  };
  const auto deserialize = omni::ryml::deserialize.tdm_map_node(customize);
  const auto result = deserialize(omni::type_t<local_mapping::record>{},
    R"({"value": 7})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  if (result.value)
    EXPECT_EQ(12, result.value->value);

  const auto parsed = omni::ryml::core::parse(R"({"value": 7})");
  EXPECT_TRUE(parsed);
  if (!parsed)
    return;

  const auto map_by_schema = omni::ryml::map_by_schema.tdm_map_node(customize);
  const auto mapped = map_by_schema(omni::type_t<local_mapping::record>{},
    *parsed);
  EXPECT_TRUE(mapped.value);
  EXPECT_TRUE(mapped.diagnostics.issues.empty());
  if (mapped.value)
    EXPECT_EQ(12, mapped.value->value);

// Aggregate deduction requires more than the C++20 language mode.
#if defined(__cpp_designated_initializers) \
  && 201707L <= __cpp_designated_initializers \
  && ((defined(__cpp_deduction_guides) && 201907L <= __cpp_deduction_guides) \
    || (defined(__clang__) && 17 <= __clang_major__))
  constexpr auto designated = omni::ryml::deserialize_t{
    .map_node = local_mapping::select_mapping{5},
  };
  const auto direct = designated(omni::type<local_mapping::record>,
    R"({"value": 7})");
  EXPECT_TRUE(direct.value);
  if (direct.value)
    EXPECT_EQ(12, direct.value->value);
#endif
}

TEST(mapping_dispatch, value_initialization_preserves_defaults) {
  constexpr omni::ryml::serialize_t<> serialize{};
  static_assert(::ryml::EmitType_e{} == serialize.format,
    "Value initialization preserves the default format");
  EXPECT_TRUE(serialization_test::matches_json("7", serialize(7)));
}

TEST(mapping_dispatch, custom_recursive_fields_can_delegate_scalar_children) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::vector<custom_depth::node>>{},
    R"([{"data": {"checked": 7, "raw": 9}, "children": []}])");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_EQ(1U, result.value->size());
    if (!result.value->empty()) {
      EXPECT_EQ(7, result.value->front().data.checked);
      EXPECT_EQ(9, result.value->front().data.raw);
    }
  }
}

// FIXME: [high] Re-enable after reducing recursive path/depth instantiations.
// Three recursive fields at depth 10 exhaust compiler memory in parallel CI
// builds, even though the terminal sentinel bounds the number of states.
#if 0
namespace bounded_depth {

struct node {
  int value;
  std::vector<node> children;
  std::vector<node> alternatives;
  std::vector<node> fallbacks;
};

std::string document(std::size_t levels) {
  std::string children = "[]";
  for (std::size_t level = 0; level < levels; ++level)
    children = R"([{"value": 7, "children": )" + children
      + R"(, "alternatives": [], "fallbacks": []}])";
  return children;
}

struct depth_case {
  std::size_t levels;
  bool accepted;
};

class traversal: public testing::TestWithParam<depth_case> {};

TEST_P(traversal, bounds_recursive_read_and_write_instantiation) {
  const auto &c = GetParam();
  const auto source = document(c.levels);
  const auto result = serialization_test::deserialize(
    omni::type_t<std::vector<node>>{}, source);

  EXPECT_EQ(c.accepted, bool(result.value));
  if (c.accepted && result.value) {
    EXPECT_TRUE(result.diagnostics.issues.empty());
    EXPECT_TRUE(serialization_test::matches_json(source,
      omni::ryml::as_json(*result.value)));
  } else if (!c.accepted) {
    EXPECT_EQ(1U, result.diagnostics.issues.size());
    if (!result.diagnostics.issues.empty()) {
      EXPECT_EQ(omni::json::issue_code::depth_limit_exceeded,
        result.diagnostics.issues.front().reason);
    }
  }
}

constexpr auto depth_limit = omni::ryml::read_state_t<>::depth_limit();
static_assert(10 == depth_limit, "The default tree-depth limit is 10");

INSTANTIATE_TEST_SUITE_P(default_limit, traversal,
  testing::Values(depth_case{depth_limit / 2, true},
    depth_case{depth_limit / 2 + 1, false}),
  [](const testing::TestParamInfo<depth_case> &info) {
    return info.param.accepted ? "at_limit" : "element_beyond_limit";
  });

TEST(traversal, root_record_reports_field_depth_overflow) {
  const auto source = document(depth_limit / 2 + 1);
  const auto result = serialization_test::deserialize(omni::type_t<node>{},
    source.substr(1, source.size() - 2));

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (!result.diagnostics.issues.empty()) {
    EXPECT_EQ(omni::json::issue_code::depth_limit_exceeded,
      result.diagnostics.issues.front().reason);
  }
}

} // namespace bounded_depth
#endif

#if 201703L <= OMNI_CPLUSPLUS
namespace schema_paths {

struct address { int number; };
struct record {
  schema_paths::address address;
  schema_paths::address work_address;
  std::vector<schema_paths::address> offices;
  std::map<std::string, schema_paths::address> locations;
};

struct read_address {
  template <typename State>
  auto operator()(State state, ::ryml::ConstNodeRef node,
    omni::type_t<address> type) const
    -> omni::ryml::with_diagnostics_t<address> {
    return omni::ryml::reflected_scope::map_object_node(
      std::move(state), node, type).transform([](address value) -> address {
        return {value.number + 100};
      });
  }
};

struct select_reader {
  template <typename State, typename T>
  auto operator()(State state, omni::type_t<T> type) const {
    if constexpr (state.path().ends_with("root.work_address"))
      return read_address{};
    else if constexpr (state.path().ends_with("root.offices[].number"))
      return local_mapping::add_integer{10};
    else if constexpr (state.path().ends_with("root.locations[].number"))
      return local_mapping::add_integer{20};
    else
      return omni::ryml::reflected_scope::fn::mapping_to_type(state, type);
  }
};

struct write_address {
  template <typename State>
  auto operator()(State state, const address &value) const -> ::ryml::NodeRef {
    return omni::ryml::reflected_scope::map_record(std::move(state),
      address{value.number + 100});
  }
};

struct write_integer {
  int increment;

  template <typename State>
  auto operator()(State state, int value) const -> ::ryml::NodeRef {
    return omni::ryml::map_arithmetic(std::move(state),
      value + this->increment);
  }
};

struct select_writer {
  template <typename State, typename T>
  auto operator()(State state, omni::type_t<T> type) const {
    if constexpr (state.path().ends_with("root.work_address"))
      return write_address{};
    else if constexpr (state.path().ends_with("root.offices[].number"))
      return write_integer{10};
    else if constexpr (state.path().ends_with("root.locations[].number"))
      return write_integer{20};
    else
      return omni::ryml::reflected_scope::fn::mapping_from_type(state, type);
  }
};

struct root {
  omni::compat::optional<std::vector<record>> records;
};

struct select_context_reader {
  template <typename State, typename T>
  auto operator()(State state, omni::type_t<T> type) const {
    if constexpr (std::is_same<typename State::enclosing_root, root>::value
      && std::is_same<typename State::enclosing_record, address>::value
      && std::is_same<T, int>::value
      && state.path().ends_with("work_address.number"))
      return local_mapping::add_integer{10};
    else
      return omni::ryml::reflected_scope::fn::mapping_to_type(state, type);
  }
};

struct select_context_writer {
  template <typename State, typename T>
  auto operator()(State state, omni::type_t<T> type) const {
    if constexpr (std::is_same<typename State::enclosing_root, root>::value
      && std::is_same<typename State::enclosing_record, address>::value
      && std::is_same<T, int>::value
      && state.path().ends_with("work_address.number"))
      return write_integer{10};
    else
      return omni::ryml::reflected_scope::fn::mapping_from_type(state, type);
  }
};

} // namespace schema_paths

namespace traversal_depth {

struct select_reader {
  template <typename State, typename T>
  auto operator()(State state, omni::type_t<T> type) const {
    if constexpr (std::is_same<T, int>::value)
      return local_mapping::add_integer{static_cast<int>(state.depth())};
    else
      return omni::ryml::reflected_scope::fn::mapping_to_type(state, type);
  }
};

struct select_writer {
  template <typename State, typename T>
  auto operator()(State state, omni::type_t<T> type) const {
    if constexpr (std::is_same<T, int>::value)
      return schema_paths::write_integer{static_cast<int>(state.depth())};
    else
      return omni::ryml::reflected_scope::fn::mapping_from_type(state, type);
  }
};

} // namespace traversal_depth

TEST(mapping_dispatch, depth_selects_read_and_write_decorations) {
  const std::string source = R"({"records": [{
    "address": {"number": 1},
    "work_address": {"number": 2},
    "offices": [{"number": 3}],
    "locations": {"remote": {"number": 4}}
  }]})";
  const auto deserialize = omni::ryml::deserialize
    .tdm_map_node([](auto) { return traversal_depth::select_reader{}; });
  const auto result = deserialize(omni::type_t<schema_paths::root>{}, source);
  EXPECT_TRUE(result.value) << result.diagnostics;
  if (!result.value)
    return;

  EXPECT_TRUE(serialization_test::matches_json(R"({"records": [{
    "address": {"number": 5},
    "work_address": {"number": 6},
    "offices": [{"number": 8}],
    "locations": {"remote": {"number": 9}}
  }]})", omni::ryml::as_json(*result.value)));

  const auto as_json = omni::ryml::as_json
    .tdm_map_node([](auto) { return traversal_depth::select_writer{}; });
  EXPECT_TRUE(serialization_test::matches_json(R"({"records": [{
    "address": {"number": 9},
    "work_address": {"number": 10},
    "offices": [{"number": 13}],
    "locations": {"remote": {"number": 14}}
  }]})", as_json(*result.value)));
  EXPECT_EQ(7, deserialize(omni::type_t<int>{}, "7").value);
  EXPECT_TRUE(serialization_test::matches_json("7", as_json(7)));
}

TEST(mapping_dispatch, schema_paths_select_reads_and_writes) {
  const std::string source = R"({
    "address": {"number": 1},
    "work_address": {"number": 2},
    "offices": [{"number": 3}],
    "locations": {"remote": {"number": 4}}
  })";
  const auto deserialize = omni::ryml::deserialize
    .tdm_map_node([](auto) { return schema_paths::select_reader{}; });
  const auto mapped = deserialize(omni::type_t<schema_paths::record>{}, source);
  EXPECT_TRUE(mapped.value);
  EXPECT_TRUE(mapped.diagnostics.empty());
  if (mapped.value) {
    EXPECT_EQ(1, mapped.value->address.number);
    EXPECT_EQ(102, mapped.value->work_address.number);
    EXPECT_EQ(13, mapped.value->offices.at(0).number);
    EXPECT_EQ(24, mapped.value->locations.at("remote").number);
  }

  const schema_paths::record value{{1}, {2}, {{3}}, {{"remote", {4}}}};
  constexpr auto as_json = omni::ryml::as_json
    .tdm_map_node([](auto) { return schema_paths::select_writer{}; });
  EXPECT_TRUE(serialization_test::matches_json(R"({
    "address": {"number": 1},
    "work_address": {"number": 102},
    "offices": [{"number": 13}],
    "locations": {"remote": {"number": 24}}
  })", as_json(value)));

// Aggregate deduction requires more than the C++20 language mode.
#if defined(__cpp_designated_initializers) \
  && 201707L <= __cpp_designated_initializers \
  && ((defined(__cpp_deduction_guides) && 201907L <= __cpp_deduction_guides) \
    || (defined(__clang__) && 17 <= __clang_major__))
  constexpr auto designated = omni::ryml::as_owning_tree_t{
    .map_node = schema_paths::select_writer{},
  };
  const auto tree = designated(value);
  EXPECT_EQ("102", tree["work_address"]["number"].val());
#endif
}

TEST(mapping_dispatch, reads_and_writes_use_parent_root_and_schema_location) {
  const std::string source = R"({"records": [{
    "address": {"number": 1},
    "work_address": {"number": 2},
    "offices": [{"number": 3}],
    "locations": {"remote": {"number": 4}}
  }]})";
  const auto deserialize = omni::ryml::deserialize
    .tdm_map_node([](auto) { return schema_paths::select_context_reader{}; });
  const auto mapped = deserialize(omni::type_t<schema_paths::root>{}, source);
  EXPECT_TRUE(mapped.value) << mapped.diagnostics;
  if (!mapped.value)
    return;

  EXPECT_TRUE(mapped.value->records);
  if (!mapped.value->records)
    return;

  EXPECT_EQ(1U, mapped.value->records->size());
  if (mapped.value->records->empty())
    return;
  const auto &record = mapped.value->records->at(0);
  EXPECT_EQ(1, record.address.number);
  EXPECT_EQ(12, record.work_address.number);
  EXPECT_EQ(3, record.offices.at(0).number);
  EXPECT_EQ(4, record.locations.at("remote").number);

  constexpr auto as_json = omni::ryml::as_json
    .tdm_map_node([](auto) { return schema_paths::select_context_writer{}; });
  EXPECT_TRUE(serialization_test::matches_json(R"({"records": [{
    "address": {"number": 1},
    "work_address": {"number": 22},
    "offices": [{"number": 3}],
    "locations": {"remote": {"number": 4}}
  }]})", as_json(*mapped.value)));

  // Parent, self, and the matching suffix stay the same; only the root changes.
  const auto independent = deserialize(omni::type_t<schema_paths::record>{},
    as_json(record));
  EXPECT_TRUE(independent.value) << independent.diagnostics;
  if (independent.value)
    EXPECT_EQ(12, independent.value->work_address.number);
}
#endif

} // namespace

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
