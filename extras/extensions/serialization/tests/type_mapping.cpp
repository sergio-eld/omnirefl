#include "json_match.hpp"

#include <cstdint>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(__GNUC__) && !defined(__clang__)
// GCC warns about GTest's internal conditionals in unbraced assertion guards.
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdangling-else"
#endif

namespace {

template <typename T>
struct endpoint { T port; };

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

struct registered_integer { int value; };

} // namespace

namespace omni {
namespace ryml {
namespace cp {

template <>
struct to_type_t<registered_integer> {
  OMNI_CONSTRAINED_TEMPLATE((typename State), (is_read_state<State>::value))
  auto operator()(State state,
    ::ryml::ConstNodeRef node,
    registered_integer *into) const
    -> with_diagnostics_t<compat::monostate> {
    return core::read_node(std::move(state), node, &into->value);
  }
};

template <>
struct from_type_t<registered_integer> {
  OMNI_CONSTRAINED_TEMPLATE((typename State), (is_write_state<State>::value))
  void operator()(State state,
    const registered_integer &value,
    ::ryml::NodeRef node) const {
    map_arithmetic(std::move(state), value.value, node);
  }
};

// C++14 supports variable-template registration and partial specialization.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <typename T>
const auto from_type<endpoint<T>> = [](auto state,
  const endpoint<T> &value,
  ::ryml::NodeRef node) {
  core::write_node(std::move(state), value.port, node);
};

template <typename T>
const auto to_type<endpoint<T>> =
  compose::with_node_check<core::expect_object_node>(
    reflected_scope::map_object_node_t<mapping::record_strategy_t<
        decltype(mapping::record_strategy.filter_fields), endpoint_names>>{});
#else
template <typename T>
struct from_type_t<endpoint<T>> {
  OMNI_CONSTRAINED_TEMPLATE((typename State), (is_write_state<State>::value))
  void operator()(State state,
    const endpoint<T> &value,
    ::ryml::NodeRef node) const {
    core::write_node(std::move(state), value.port, node);
  }
};

// C++11 registers a struct and configures the default compositor at invocation.
template <typename T>
struct to_type_t<endpoint<T>>:
    decltype(compose::with_node_check<core::expect_object_node>(
      reflected_scope::map_object_node_t<mapping::record_strategy_t<
        decltype(mapping::record_strategy.filter_fields), endpoint_names>>{})) {};
#endif

// Initialized captures must survive dispatcher selection in both directions.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <>
const auto to_type<shifted_integer> =
  [bias = 7](auto state, ::ryml::ConstNodeRef node, shifted_integer *into) {
    int decoded{};
    auto result = map_integer_node(std::move(state), node, &decoded);
    if (result.value)
      into->value = decoded + bias;

    return result;
  };

template <>
const auto from_type<shifted_integer> =
  [bias = 11](auto state, const shifted_integer &value, ::ryml::NodeRef node) {
    map_integral(std::move(state), value.value + bias, node);
  };
#else
// C++11 uses named callable types instead of capturing generic lambdas.
template <>
struct to_type_t<shifted_integer> {
  int bias;
  constexpr to_type_t(): bias(7) {}

  OMNI_CONSTRAINED_TEMPLATE((typename State), (is_read_state<State>::value))
  auto operator()(State state,
    ::ryml::ConstNodeRef node,
    shifted_integer *into) const -> with_diagnostics_t<compat::monostate> {
    int decoded{};
    auto result = map_integer_node(std::move(state), node, &decoded);
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
  void operator()(State state,
    const shifted_integer &value,
    ::ryml::NodeRef node) const {
    map_integral(std::move(state), value.value + this->bias, node);
  }
};
#endif

// Registration lookup must preserve T's cv-qualification.
template <>
struct from_type_t<const qualified_integer> {
  OMNI_CONSTRAINED_TEMPLATE((typename State), (is_write_state<State>::value))
  void operator()(State state,
    const qualified_integer &value,
    ::ryml::NodeRef node) const {
    map_integral(std::move(state), value.value, node);
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
    != omni::ryml::cp::to_type_t<registered_integer>{},
  "Registered structs do not compare equal to unavailable");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<bool>{})),
    omni::ryml::map_boolean_node_t<>>::value,
  "Boolean precedes the integral mapping");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<int>{})),
    omni::ryml::map_integer_node_t<>>::value,
  "Signed integers use the integer mapping");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<unsigned>{})),
    omni::ryml::map_integer_node_t<>>::value,
  "Unsigned integers use the integer mapping");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<double>{})),
    omni::ryml::map_number_node_t<>>::value,
  "Floating-point values use the real mapping");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<bool>{})),
    omni::ryml::map_boolean_t>::value,
  "Boolean precedes the integral writer");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<int>{})),
    omni::ryml::map_integral_t>::value,
  "Signed integers use the integer writer");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<unsigned>{})),
    omni::ryml::map_integral_t>::value,
  "Unsigned integers use the integer writer");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<double>{})),
    omni::ryml::map_floating_point_t>::value,
  "Floating-point values use the real writer");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<std::string>{})),
    omni::ryml::map_string_node_t<>
    >::value,
  "String to-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<std::string>{})),
    omni::ryml::map_string_like_t>::value,
  "String from-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<std::vector<int>>{})),
    omni::ryml::map_range_node_t<>>::value,
  "Range to-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<std::vector<int>>{})),
    omni::ryml::map_range_t>::value,
  "Range from-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<omni::compat::optional<int>>{})),
    omni::ryml::map_value_node_t<>>::value,
  "Optional to-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<omni::compat::optional<int>>{})),
    omni::ryml::map_optional_like_t>::value,
  "Optional from-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<registered_integer>{})),
    omni::ryml::cp::to_type_t<registered_integer>>::value,
  "Struct registration to-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<registered_integer>{})),
    omni::ryml::cp::from_type_t<registered_integer>>::value,
  "Struct registration from-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<const bool>{})),
    omni::ryml::map_boolean_node_t<>>::value,
  "const bool uses the Boolean to-type mapping");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<const bool>{})),
    omni::ryml::map_boolean_t>::value,
  "const bool uses the Boolean from-type mapping");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<volatile bool>{})),
    omni::ryml::map_boolean_node_t<>>::value,
  "volatile bool uses the Boolean to-type mapping");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<volatile bool>{})),
    omni::ryml::map_boolean_t>::value,
  "volatile bool uses the Boolean from-type mapping");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<const volatile bool>{})),
    omni::ryml::map_boolean_node_t<>>::value,
  "const volatile bool uses the Boolean to-type mapping");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<const volatile bool>{})),
    omni::ryml::map_boolean_t>::value,
  "const volatile bool uses the Boolean from-type mapping");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<const std::string>{})),
    omni::ryml::map_string_node_t<>
    >::value,
  "Qualified string to-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<const std::string>{})),
    omni::ryml::map_string_like_t>::value,
  "Qualified string from-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<const std::vector<int>>{})),
    omni::ryml::map_range_node_t<>>::value,
  "Qualified range to-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<const std::vector<int>>{})),
    omni::ryml::map_range_t>::value,
  "Qualified range from-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_to_type(
      omni::type_t<const omni::compat::optional<int>>{})),
    omni::ryml::map_value_node_t<>>::value,
  "Qualified optional to-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<const omni::compat::optional<int>>{})),
    omni::ryml::map_optional_like_t>::value,
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

TEST(mapping_core, parses_and_converts_through_explicit_core_mappings) {
  const auto tree = omni::ryml::core::parse(
    R"({"flag": true, "count": 42, "ratio": 2.5, "text": "reading"})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const omni::ryml::config_t<> options{};
  const auto root = tree->crootref();
  const auto boolean = serialization_test::map_using(options,
    omni::ryml::core::map_node_to_boolean,
    omni::type_t<bool>{}, root["flag"]);

  const auto integer = serialization_test::map_using(options,
    omni::ryml::core::map_node_to_integral,
    omni::type_t<std::int16_t>{}, root["count"]);

  const auto real = serialization_test::map_using(options,
    omni::ryml::core::map_node_to_floating_point,
    omni::type_t<float>{}, root["ratio"]);

  const auto arithmetic = serialization_test::map_using(options,
    omni::ryml::core::map_node_to_arithmetic,
    omni::type_t<std::int32_t>{}, root["count"]);

  const auto text = serialization_test::map_using(options,
    omni::ryml::core::map_node_to_string_like,
    omni::type_t<std::string>{}, root["text"]);

  EXPECT_EQ(true, boolean.value);
  EXPECT_EQ(42, integer.value);
  EXPECT_EQ(2.5F, real.value);
  EXPECT_EQ(42, arithmetic.value);
  EXPECT_EQ(std::string{"reading"}, text.value);
  EXPECT_TRUE(boolean.diagnostics.issues.empty());
  EXPECT_TRUE(integer.diagnostics.issues.empty());
  EXPECT_TRUE(real.diagnostics.issues.empty());
  EXPECT_TRUE(arithmetic.diagnostics.issues.empty());
  EXPECT_TRUE(text.diagnostics.issues.empty());
}

TEST(mapping_core, reports_overflow_at_the_nodes_document_path) {
  const auto tree = omni::ryml::core::parse(R"({"count": 128})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const omni::ryml::config_t<> options{};
  const auto node = tree->crootref()["count"];
  const auto result = serialization_test::map_using(options,
    omni::ryml::core::map_node_to_integral, omni::type_t<std::int8_t>{}, node);

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (result.diagnostics.issues.size() == 1) {
    EXPECT_EQ(omni::json::issue_code::out_of_range,
      result.diagnostics.issues[0].reason);

    EXPECT_EQ("/count", omni::ryml::detail::render_path(
      result.diagnostics.issues[0].path));
  }
}

TEST(type_mapping, selects_reflected_defaults) {
  omni::reflected_call(cpp11_check_mapping_types<
      omni::ryml::reflected_scope::map_object_node_t<>,
      omni::ryml::reflected_scope::map_record_t<>>{},
    omni::type_t<uncustomized_record>{});
  omni::reflected_call(cpp11_check_mapping_types<
      omni::ryml::reflected_scope::map_string_node_t<>,
      omni::ryml::reflected_scope::map_enum_t<>>{},
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

// C++11 needs a named generic selector to retain the explicitly queried tag.
template <typename Qualified>
struct select_qualified_writer {
  OMNI_CONSTRAINED_TEMPLATE((typename State, typename T),
    (omni::ryml::is_write_state<State>::value))
  auto operator()(State, omni::type_t<T>) const
    -> decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<Qualified>{})) {
    return omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<Qualified>{});
  }
};

TEST(type_mapping, writes_cv_qualified_boolean_tags) {
  const bool enabled = true;
  volatile bool disabled = false;
  const volatile bool fixed = true;
  const auto write_const = omni::ryml::as_owning_tree.tdm_map_node(
    [](omni::ryml::reflected_scope::mapping_from_type_t) {
      return select_qualified_writer<const bool>{};
    });
  const auto write_volatile = omni::ryml::as_owning_tree.tdm_map_node(
    [](omni::ryml::reflected_scope::mapping_from_type_t) {
      return select_qualified_writer<volatile bool>{};
    });
  const auto write_const_volatile = omni::ryml::as_owning_tree.tdm_map_node(
    [](omni::ryml::reflected_scope::mapping_from_type_t) {
      return select_qualified_writer<const volatile bool>{};
    });

  EXPECT_EQ("true", ::ryml::emitrs_json<std::string>(write_const(enabled)));
  EXPECT_EQ("false",
    ::ryml::emitrs_json<std::string>(write_volatile(disabled)));
  EXPECT_EQ("true",
    ::ryml::emitrs_json<std::string>(write_const_volatile(fixed)));
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
  const auto as_owning_tree = omni::ryml::as_owning_tree.tdm_map_node(
    [](omni::ryml::reflected_scope::mapping_from_type_t) {
      return select_qualified_writer<const qualified_integer>{};
    });

  EXPECT_EQ("23", ::ryml::emitrs_json<std::string>(as_owning_tree(value)));
}

TEST(write_state, preserves_string_storage_through_custom_mapping) {
  std::vector<endpoint<std::string>> source{{"east"}, {"west"}};
  const auto owning = omni::ryml::as_owning_tree(source);

  {
    const auto borrowed = omni::ryml::as_ref_tree(source);
    EXPECT_TRUE(serialization_test::matches_node(
      R"(["east", "west"])", borrowed.crootref()));

    EXPECT_EQ(static_cast<const void *>(source.front().port.data()),
      static_cast<const void *>(borrowed.crootref()[0].val().str));
  }

  source.clear();
  EXPECT_TRUE(serialization_test::matches_node(
    R"(["east", "west"])", owning.crootref()));
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
