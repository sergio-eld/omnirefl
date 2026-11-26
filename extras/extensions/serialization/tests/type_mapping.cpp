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
    return map_integer_node(std::move(state), node, &into->value);
  }
};

template <>
struct from_type_t<registered_integer> {
  OMNI_CONSTRAINED_TEMPLATE((typename State), (is_write_state<State>::value))
  auto operator()(State state,
    const registered_integer &value) const -> ::ryml::NodeRef {
    return map_arithmetic(std::move(state), value.value);
  }
};

// C++14 supports variable-template registration and partial specialization.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <typename T>
const auto from_type<endpoint<T>> = [](auto state,
  const endpoint<T> &value) -> ::ryml::NodeRef {
  const auto mapping = compat::invoke(state.map_node,
    state, omni::type_t<T>{});
  return mapping(std::move(state), value.port);
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
  auto operator()(State state,
    const endpoint<T> &value) const -> ::ryml::NodeRef {
    const auto mapping = compat::invoke(state.map_node,
      state, omni::type_t<T>{});
    return mapping(std::move(state), value.port);
  }
};

// C++11 registers a struct and configures the default compositor at invocation.
template <typename T>
struct to_type_t<endpoint<T>> {
  decltype(compose::with_node_check<core::expect_object_node>(
    reflected_scope::map_object_node_t<mapping::record_strategy_t<
      decltype(mapping::record_strategy.filter_fields),
      endpoint_names>>{})) mapping{};

  template <typename... Args>
  auto operator()(Args &&...args) const
    -> decltype(this->mapping(std::forward<Args>(args)...)) {
    return this->mapping(std::forward<Args>(args)...);
  }
};
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
  [bias = 11](auto state, const shifted_integer &value) -> ::ryml::NodeRef {
    return map_integral(std::move(state), value.value + bias);
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
  auto operator()(State state,
    const shifted_integer &value) const -> ::ryml::NodeRef {
    return map_integral(std::move(state), value.value + this->bias);
  }
};
#endif

// Registration lookup normalizes qualified type tags.
template <>
struct from_type_t<qualified_integer> {
  OMNI_CONSTRAINED_TEMPLATE((typename State), (is_write_state<State>::value))
  auto operator()(State state,
    const qualified_integer &value) const -> ::ryml::NodeRef {
    return map_integral(std::move(state), value.value);
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
      omni::type_t<registered_integer>{})),
    omni::ryml::cp::to_type_t<registered_integer>>::value,
  "Struct registration to-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<registered_integer>{})),
    omni::ryml::cp::from_type_t<registered_integer>>::value,
  "Struct registration from-type selection");

static_assert(std::is_same<
    decltype(omni::ryml::reflected_scope::mapping_from_type(
      omni::type_t<const qualified_integer>{})),
    omni::ryml::cp::from_type_t<qualified_integer>>::value,
  "Registration lookup normalizes the qualified type");

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

TEST(mapping_core, parses_and_converts_through_explicit_core_mappings) {
  const auto tree = omni::ryml::core::parse(
    R"({"flag": true, "count": 42, "ratio": 2.5, "text": "reading"})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto root = tree->crootref();
  const auto boolean = omni::ryml::core::parse_as_boolean(root["flag"]);
  const auto integer = omni::ryml::core::parse_as_integral<std::int16_t>(
    root["count"]);
  const auto real = omni::ryml::core::parse_as_floating_point<float>(
    root["ratio"]);
  const auto arithmetic = omni::ryml::core::parse_as_arithmetic<std::int32_t>(
    root["count"]);
  const auto text = omni::ryml::core::parse_as_string_like<std::string>(
    root["text"]);

  EXPECT_TRUE(boolean);
  EXPECT_TRUE(integer);
  EXPECT_TRUE(real);
  EXPECT_TRUE(arithmetic);
  EXPECT_TRUE(text);
  if (boolean)
    EXPECT_EQ(true, *boolean);
  if (integer)
    EXPECT_EQ(42, *integer);
  if (real)
    EXPECT_EQ(2.5F, *real);
  if (arithmetic)
    EXPECT_EQ(42, *arithmetic);
  if (text)
    EXPECT_EQ(std::string{"reading"}, *text);
}

TEST(mapping_core, reports_overflow_at_the_nodes_document_path) {
  const auto tree = omni::ryml::core::parse(R"({"count": 128})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto node = tree->crootref()["count"];
  const auto result = omni::ryml::core::parse_as_integral<std::int8_t>(node);

  EXPECT_FALSE(result);
  if (!result) {
    EXPECT_EQ(omni::json::issue_code::out_of_range, result.error().reason);
    EXPECT_EQ("/count", omni::ryml::detail::render_path(result.error().path));
  }
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

TEST(type_mapping, normalizes_qualified_registration_lookup) {
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
