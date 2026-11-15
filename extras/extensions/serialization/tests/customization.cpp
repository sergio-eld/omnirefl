#include "json_match.hpp"

#include <tuple>
#include <vector>

#if defined(__GNUC__) && !defined(__clang__)
// GCC warns about GTest's internal conditionals in unbraced assertion guards.
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdangling-else"
#endif

namespace {

template <typename T>
struct endpoint { T port; };

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
struct from_type_t<mapping_observation::number> {
  template <bool BorrowStrings>
  void operator()(const mapping_observation::number &value,
    ::ryml::NodeRef node, write_state_t<BorrowStrings> state) const {
    core::write_node(value.value, node, state);
  }
};

template <>
struct to_type_t<mapping_observation::number> {
  template <typename Config>
  auto operator()(::ryml::ConstNodeRef node,
    mapping_observation::number *into, read_state_t<Config> state) const
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

// C++14 supports variable-template registration and partial specialization.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <typename T>
const auto from_type<endpoint<T>> = [](const endpoint<T> &value,
  ::ryml::NodeRef node, auto state) {
  core::write_node(value.port, node, state);
};

template <typename T>
const auto to_type<endpoint<T>> = reflected_scope::to_record.with_name_by_alias(
  alias_dict(std::make_tuple("port", "service_port")));
#else
template <typename T>
struct from_type_t<endpoint<T>> {
  template <bool BorrowStrings>
  void operator()(const endpoint<T> &value,
    ::ryml::NodeRef node, write_state_t<BorrowStrings> state) const {
    core::write_node(value.port, node, state);
  }
};

// C++11 registers a struct and configures the default compositor at invocation.
template <typename T>
struct to_type_t<endpoint<T>> {
  template <typename Config>
  auto operator()(::ryml::ConstNodeRef node,
    endpoint<T> *into,
    omni::ryml::read_state_t<Config> context) const
    -> with_diagnostics_t<compat::monostate, diagnostics> {
    return reflected_scope::to_record.with_name_by_alias(
      [](compat::string_view alias) -> compat::optional<compat::string_view> {
        return alias == "service_port"
          ? compat::optional<compat::string_view>{"port"}
          : compat::nullopt;
      })(node, into, context);
  }
};
#endif

} // namespace cp
} // namespace ryml
} // namespace omni

namespace {

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
