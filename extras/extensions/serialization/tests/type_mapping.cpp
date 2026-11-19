#include "json_match.hpp"

#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#if defined(__GNUC__) && !defined(__clang__)
// GCC warns about GTest's internal conditionals in unbraced assertion guards.
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdangling-else"
#endif

namespace {

template <typename T>
struct endpoint { T port; };

// TODO: [high] Move this alias fixture to the model customization API.
struct endpoint_names {
  omni::compat::optional<omni::compat::string_view> operator()(
    omni::compat::string_view alias) const {
    return alias == "service_port"
      ? omni::compat::optional<omni::compat::string_view>{"port"}
      : omni::compat::nullopt;
  }
};

struct shifted_integer { int value; };
struct qualified_integer { int value; };

namespace mapping_observation {

struct trace {
  std::vector<std::string> paths;
  std::vector<std::size_t> errors;
  std::vector<std::size_t> warnings;
  std::vector<const void *> configurations;
};

struct renderer {
  trace *observed;

  std::size_t operator()(const ::ryml::Tree &,
    omni::ryml::diagnostics report) const {
    return report.issues.size();
  }
};

struct number { int value; };
struct record {
  number first;
  number second;
  number third;
};

} // namespace mapping_observation


} // namespace

namespace omni {
namespace ryml {
namespace cp {

template <>
struct to_type_t<mapping_observation::number> {
  OMNI_CONSTRAINED_TEMPLATE((typename State), (is_read_state<State>::value))
  auto operator()(::ryml::ConstNodeRef node,
    mapping_observation::number *into, State state) const
    -> with_diagnostics_t<compat::monostate> {
    auto &trace = *state.config.transform_diagnostics.observed;
    trace.paths.push_back(detail::render_path(state.path.materialize()));
    trace.errors.push_back(state.error_count);
    trace.warnings.push_back(state.warning_count);
    trace.configurations.push_back(&state.config);

    auto result = core::read_node(node, &into->value, state);
    if (result.value && into->value < 0) {
      result.diagnostics.issues.push_back(detail::make_issue(
        json::issue_code::out_of_range, json::value_kind::integer,
        node, state.path, /*warning=*/true));
    }

    return result;
  }
};

template <>
struct from_type_t<mapping_observation::number> {
  OMNI_CONSTRAINED_TEMPLATE((typename State), (is_write_state<State>::value))
  void operator()(const mapping_observation::number &value,
    ::ryml::NodeRef node, State state) const {
    from_arithmetic(value.value, node, state);
  }
};

// C++14 supports variable-template registration and partial specialization.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <typename T>
const auto from_type<endpoint<T>> = [](const endpoint<T> &value,
  ::ryml::NodeRef node, auto state) {
  core::write_node(value.port, node, state);
};

template <typename T>
const auto to_type<endpoint<T>> =
  compose::with_node_check<core::expect_object_node>(
    reflected_scope::to_record_t<endpoint_names>{});
#else
template <typename T>
struct from_type_t<endpoint<T>> {
  OMNI_CONSTRAINED_TEMPLATE((typename State), (is_write_state<State>::value))
  void operator()(const endpoint<T> &value,
    ::ryml::NodeRef node, State state) const {
    core::write_node(value.port, node, state);
  }
};

// C++11 registers a struct and configures the default compositor at invocation.
template <typename T>
struct to_type_t<endpoint<T>>:
    decltype(compose::with_node_check<core::expect_object_node>(
      reflected_scope::to_record_t<endpoint_names>{})) {};
#endif

// Initialized captures must survive dispatcher selection in both directions.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <>
const auto to_type<shifted_integer> =
  [bias = 7](::ryml::ConstNodeRef node, shifted_integer *into, auto state) {
    int decoded{};
    auto result = default_to_integer(omni::type_t<int>{})(
      node, &decoded, state);
    if (result.value)
      into->value = decoded + bias;

    return result;
  };

template <>
const auto from_type<shifted_integer> =
  [bias = 11](const shifted_integer &value, ::ryml::NodeRef node, auto state) {
    from_integer(value.value + bias, node, state);
  };
#else
// C++11 uses named callable types instead of capturing generic lambdas.
template <>
struct to_type_t<shifted_integer> {
  int bias;
  constexpr to_type_t(): bias(7) {}

  OMNI_CONSTRAINED_TEMPLATE((typename State), (is_read_state<State>::value))
  auto operator()(::ryml::ConstNodeRef node, shifted_integer *into,
    State state) const -> with_diagnostics_t<compat::monostate> {
    int decoded{};
    auto result = default_to_integer(omni::type_t<int>{})(
      node, &decoded, state);
    if (result.value)
      into->value = decoded + this->bias;

    return result;
  }
};

template <>
struct from_type_t<shifted_integer> {
  int bias;
  constexpr from_type_t(): bias(11) {}

  OMNI_CONSTRAINED_TEMPLATE((typename State), (is_write_state<State>::value))
  void operator()(const shifted_integer &value, ::ryml::NodeRef node,
    State state) const {
    from_integer(value.value + this->bias, node, state);
  }
};
#endif

// Registration lookup must preserve T's cv-qualification.
template <>
struct from_type_t<const qualified_integer> {
  OMNI_CONSTRAINED_TEMPLATE((typename State), (is_write_state<State>::value))
  void operator()(const qualified_integer &value, ::ryml::NodeRef node,
    State state) const {
    from_integer(value.value, node, state);
  }
};

} // namespace cp
} // namespace ryml
} // namespace omni

namespace {

static_assert(omni::traits::cp::unavailable{}
    == omni::ryml::cp::to_type_t<int>{},
  "Unregistered structs compare equal to unavailable");
static_assert(omni::traits::cp::unavailable{}
    != omni::ryml::cp::to_type_t<mapping_observation::number>{},
  "Registered structs do not compare equal to unavailable");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<bool>{})),
    decltype(omni::ryml::default_to_boolean(omni::type_t<bool>{}))>::value,
  "Boolean precedes the integral mapping");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<int>{})),
    decltype(omni::ryml::default_to_integer(omni::type_t<int>{}))>::value,
  "Signed integers use the integer mapping");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<unsigned>{})),
    decltype(omni::ryml::default_to_integer(omni::type_t<int>{}))>::value,
  "Unsigned integers use the integer mapping");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<double>{})),
    decltype(omni::ryml::default_to_real(omni::type_t<double>{}))>::value,
  "Floating-point values use the real mapping");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<bool>{})),
    omni::ryml::from_boolean_t>::value,
  "Boolean precedes the integral writer");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<int>{})),
    omni::ryml::from_integer_t>::value,
  "Signed integers use the integer writer");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<unsigned>{})),
    omni::ryml::from_integer_t>::value,
  "Unsigned integers use the integer writer");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<double>{})),
    omni::ryml::from_real_t>::value,
  "Floating-point values use the real writer");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<std::string>{})),
    decltype(omni::ryml::default_to_string_like(omni::type_t<std::string>{}))
    >::value,
  "String to-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<std::string>{})),
    omni::ryml::from_string_like_t>::value,
  "String from-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<std::vector<int>>{})),
    decltype(omni::ryml::default_to_range(
      omni::type_t<std::vector<int>>{}))>::value,
  "Range to-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<std::vector<int>>{})),
    omni::ryml::from_range_t>::value,
  "Range from-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<omni::compat::optional<int>>{})),
    decltype(omni::ryml::default_to_optional_like(
      omni::type_t<omni::compat::optional<int>>{}))>::value,
  "Optional to-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<omni::compat::optional<int>>{})),
    omni::ryml::from_optional_like_t>::value,
  "Optional from-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<mapping_observation::number>{})),
    omni::ryml::cp::to_type_t<mapping_observation::number>>::value,
  "Struct registration to-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<mapping_observation::number>{})),
    omni::ryml::cp::from_type_t<mapping_observation::number>>::value,
  "Struct registration from-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<const bool>{})),
    decltype(omni::ryml::default_to_boolean(omni::type_t<bool>{}))>::value,
  "const bool uses the Boolean to-type mapping");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<const bool>{})),
    omni::ryml::from_boolean_t>::value,
  "const bool uses the Boolean from-type mapping");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<volatile bool>{})),
    decltype(omni::ryml::default_to_boolean(omni::type_t<bool>{}))>::value,
  "volatile bool uses the Boolean to-type mapping");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<volatile bool>{})),
    omni::ryml::from_boolean_t>::value,
  "volatile bool uses the Boolean from-type mapping");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<const volatile bool>{})),
    decltype(omni::ryml::default_to_boolean(omni::type_t<bool>{}))>::value,
  "const volatile bool uses the Boolean to-type mapping");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<const volatile bool>{})),
    omni::ryml::from_boolean_t>::value,
  "const volatile bool uses the Boolean from-type mapping");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<const std::string>{})),
    decltype(omni::ryml::default_to_string_like(omni::type_t<std::string>{}))
    >::value,
  "Qualified string to-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<const std::string>{})),
    omni::ryml::from_string_like_t>::value,
  "Qualified string from-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<const std::vector<int>>{})),
    decltype(omni::ryml::default_to_range(
      omni::type_t<std::vector<int>>{}))>::value,
  "Qualified range to-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<const std::vector<int>>{})),
    omni::ryml::from_range_t>::value,
  "Qualified range from-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<const omni::compat::optional<int>>{})),
    decltype(omni::ryml::default_to_optional_like(
      omni::type_t<omni::compat::optional<int>>{}))>::value,
  "Qualified optional to-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<const omni::compat::optional<int>>{})),
    omni::ryml::from_optional_like_t>::value,
  "Qualified optional from-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<const qualified_integer>{})),
    omni::ryml::cp::from_type_t<const qualified_integer>>::value,
  "Registration lookup preserves the original qualified type");

// Compare registrations with the registered objects, not a return-type helper.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<endpoint<int>>{})),
    omni::compat::remove_cvref_t<decltype(
      omni::ryml::cp::to_type<endpoint<int>>)>>::value,
  "Variable registration selects its callable type");
static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<endpoint<int>>{})),
    omni::compat::remove_cvref_t<decltype(
      omni::ryml::cp::from_type<endpoint<int>>)>>::value,
  "Variable registration selects its writer type");
#else
static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<endpoint<int>>{})),
    omni::ryml::cp::to_type_t<endpoint<int>>>::value,
  "C++11 template registration selects its reader type");
static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<endpoint<int>>{})),
    omni::ryml::cp::from_type_t<endpoint<int>>>::value,
  "C++11 template registration selects its writer type");
#endif

struct uncustomized_record { int value; };
enum class uncustomized_enum { value };

// C++11 lambda equivalent: expected types are independent of the dispatcher.
template <typename Reader, typename Writer>
struct cpp11_check_mapping_types {
  template <typename _M, omni::reflected_entity Entity>
  void operator()(omni::meta_t<_M, Entity>) const {
// Reflected defaults can be queried only after metadata generation.
#if !defined(OMNI_TOOL_RUN)
    using T = typename omni::meta_t<_M, Entity>::reflected_type;
    static_assert(std::is_same<decltype(
        omni::ryml::reflected_scope::mapping_to_type(omni::type_t<T>{})),
        Reader>::value,
      "Reflected reader selection");
    static_assert(std::is_same<decltype(
        omni::ryml::reflected_scope::mapping_from_type(omni::type_t<T>{})),
        Writer>::value,
      "Reflected writer selection");
#endif
  }
};

TEST(type_mapping, selects_reflected_defaults) {
  omni::reflected_call(cpp11_check_mapping_types<
      decltype(omni::ryml::reflected_scope::default_to_record(
        omni::type_t<uncustomized_record>{})),
      omni::ryml::reflected_scope::from_record_t>{},
    omni::type_t<uncustomized_record>{});
  omni::reflected_call(cpp11_check_mapping_types<
      decltype(omni::ryml::reflected_scope::default_to_enum(
        omni::type_t<uncustomized_enum>{})),
      omni::ryml::reflected_scope::from_enum_t>{},
    omni::type_t<uncustomized_enum>{});
}

TEST(type_mapping, preserves_initialized_reader_state) {
  const auto result = omni::ryml::deserialize(
    omni::type_t<shifted_integer>{}, "5");

  EXPECT_TRUE(result.value);
  if (result.value)
    EXPECT_EQ(12, result.value->value);
}

TEST(type_mapping, preserves_initialized_writer_state) {
  EXPECT_EQ("16", omni::ryml::as_json(shifted_integer{5}));
}

TEST(type_mapping, writes_cv_qualified_boolean_tags) {
  const bool enabled = true;
  volatile bool disabled = false;
  const volatile bool fixed = true;
  const omni::ryml::write_state_t<false> state{};
  ::ryml::Tree tree;

  omni::ryml::reflected_scope::mapping_from_type(omni::type_t<const bool>{})(
    enabled, tree.rootref(), state);
  EXPECT_EQ("true", ::ryml::emitrs_json<std::string>(tree));

  omni::ryml::reflected_scope::mapping_from_type(omni::type_t<volatile bool>{})(
    disabled, tree.rootref(), state);
  EXPECT_EQ("false", ::ryml::emitrs_json<std::string>(tree));

  omni::ryml::reflected_scope::mapping_from_type(
    omni::type_t<const volatile bool>{})(fixed, tree.rootref(), state);
  EXPECT_EQ("true", ::ryml::emitrs_json<std::string>(tree));
}

TEST(type_mapping, reads_a_volatile_boolean) {
  volatile bool into = false;
  const auto result = omni::ryml::deserialize(&into, "true");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  EXPECT_TRUE(into);
}

TEST(type_mapping, preserves_qualified_registration) {
  const qualified_integer value{23};
  const omni::ryml::write_state_t<false> state{};
  ::ryml::Tree tree;

  omni::ryml::reflected_scope::mapping_from_type(
    omni::type_t<const qualified_integer>{})(value, tree.rootref(), state);
  EXPECT_EQ("23", ::ryml::emitrs_json<std::string>(tree));
}

TEST(write_state, preserves_string_storage_through_custom_mapping) {
  std::vector<endpoint<std::string>> source{{"east"}, {"west"}};
  const auto owning = omni::ryml::core::as_owning_tree(source);

  {
    const auto borrowed = omni::ryml::core::as_ref_tree(source);
    EXPECT_TRUE(serialization_test::matches_node(
      R"(["east", "west"])", borrowed.crootref()));

    EXPECT_EQ(static_cast<const void *>(source.front().port.data()),
      static_cast<const void *>(borrowed.crootref()[0].val().str));
  }

  source.clear();
  EXPECT_TRUE(serialization_test::matches_node(
    R"(["east", "west"])", owning.crootref()));
}

TEST(read_state, passes_original_config_and_updated_counts_to_children) {
  mapping_observation::trace trace;
  omni::ryml::config_t<mapping_observation::renderer> options{
    /*error_policy=*/{},
    /*transform_diagnostics=*/{&trace},
  };

  options.error_policy.error_limit = 1;
  const auto result = omni::ryml::core::deserialize(options,
    omni::type_t<mapping_observation::record>{},
    R"({"first":-7,"second":"bad","third":3})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(2U, result.diagnostics);
  EXPECT_EQ((std::vector<std::string>{"/first", "/second", "/third"}),
    trace.paths);

  EXPECT_EQ((std::vector<std::size_t>{0, 0, 1}), trace.errors);
  EXPECT_EQ((std::vector<std::size_t>{0, 1, 1}), trace.warnings);
  EXPECT_EQ(trace.paths.size(), trace.configurations.size());
  for (const auto address : trace.configurations)
    EXPECT_EQ(static_cast<const void *>(&options), address);
}

class record_mapping_limits: public testing::TestWithParam<bool> {};

TEST_P(record_mapping_limits, stops_before_evaluating_later_members) {
  const bool warnings = GetParam();
  mapping_observation::trace trace;
  omni::ryml::config_t<mapping_observation::renderer> options{
    /*error_policy=*/{},
    /*transform_diagnostics=*/{&trace},
  };

  options.error_policy.error_limit = 0;
  if (warnings)
    options.error_policy.warning_limit = 0;

  const auto result = omni::ryml::core::deserialize(options,
    omni::type_t<mapping_observation::record>{},
    warnings ? R"({"first":-7,"second":2,"third":3})"
             : R"({"first":"bad","second":2,"third":3})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(warnings ? 2U : 1U, result.diagnostics);
  EXPECT_EQ((std::vector<std::string>{"/first"}), trace.paths);
  EXPECT_EQ((std::vector<std::size_t>{0}), trace.errors);
  EXPECT_EQ((std::vector<std::size_t>{0}), trace.warnings);
}

INSTANTIATE_TEST_SUITE_P(budgets,
  record_mapping_limits,
  testing::Bool(),
  [](const testing::TestParamInfo<bool> &info) {
    return info.param ? "Warnings" : "Errors";
  });

TEST(record_mapping_events, missing_members_stop_before_value_operations) {
  mapping_observation::trace trace;
  const omni::ryml::config_t<mapping_observation::renderer> options{
    /*error_policy=*/{},
    /*transform_diagnostics=*/{&trace},
  };

  const auto result = omni::ryml::core::deserialize(options,
    omni::type_t<mapping_observation::record>{},
    R"({"first":1,"third":3})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics);
  EXPECT_TRUE(trace.paths.empty());
}

TEST(mapping_customization, selects_serialization_registration) {
  const endpoint<int> source{/*port=*/42};

  EXPECT_EQ("42", omni::ryml::as_json(source));
}

TEST(mapping_customization, preserves_configured_alias_lookup) {
  const auto result = serialization_test::deserialize(
    omni::type_t<endpoint<int>>{}, R"({"service_port": 42})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_EQ(42, result.value->port);
}

} // namespace

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
