#include "json_match.hpp"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#if defined(__GNUC__) && !defined(__clang__)
// GCC warns about GTest's internal conditionals in unbraced assertion guards.
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdangling-else"
#endif

namespace {
namespace inplace_models {

enum class mode { idle, busy };

struct record {
  std::string name;
  std::vector<int> samples;
  std::map<std::string, int> ports;
};

struct constructed_record {
  explicit constructed_record(int initial): value(initial) {}
  constructed_record(const constructed_record &) = delete;
  constructed_record &operator=(const constructed_record &) = delete;
  int value;
};

struct nonassignable_record {
  int value;

  nonassignable_record &operator=(const nonassignable_record &) = delete;
};

struct readings {
  int first;
  int second;
};

} // namespace inplace_models

TEST(inplace_records, maps_fields_without_assigning_the_root) {
  inplace_models::nonassignable_record into{7};
  const auto result = omni::ryml::deserialize(&into, R"({"value": 42})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  EXPECT_EQ(42, into.value);
}

struct inplace_record_case {
  std::string name;
  inplace_models::record initial;
  std::string document;
  inplace_models::record expected;
};

class inplace_records: public testing::TestWithParam<inplace_record_case> {};

TEST_P(inplace_records, replaces_existing_values_and_containers) {
  const auto &c = GetParam();
  auto into = c.initial;
  const auto result = omni::ryml::deserialize(&into, c.document);

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  EXPECT_EQ(c.expected.name, into.name);
  EXPECT_EQ(c.expected.samples, into.samples);
  EXPECT_EQ(c.expected.ports, into.ports);
}

INSTANTIATE_TEST_SUITE_P(documents, inplace_records,
  testing::ValuesIn(std::vector<inplace_record_case>{
    {
      "json",
      {"old", {99, 100}, {{"api", 99}, {"stale", 1}}},
      R"(
{
  "name": "new",
  "samples": [7, 42],
  "ports": {"api": 443}
}
)",
      {"new", {7, 42}, {{"api", 443}}},
    },
    {
      "yaml",
      {"old", {99, 100}, {{"api", 99}, {"stale", 1}}},
      R"(
name: new
samples: [7, 42]
ports:
  api: 443
)",
      {"new", {7, 42}, {{"api", 443}}},
    },
    {
      "empty_containers",
      {"old", {99, 100}, {{"api", 99}, {"stale", 1}}},
      R"({"name": "new", "samples": [], "ports": {}})",
      {"new", {}, {}},
    },
  }),

  [](const testing::TestParamInfo<inplace_record_case> &info) {
    return info.param.name;
  });

TEST(inplace_mapping, updates_a_record_without_constructing_or_assigning_it) {
  const auto tree = omni::ryml::core::parse(R"({"value": 42})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  inplace_models::constructed_record into{7};
  const auto result = omni::ryml::map_by_schema(&into, *tree);

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  EXPECT_EQ(42, into.value);
}

TEST(inplace_mapping, replaces_a_populated_map_from_a_subtree) {
  const auto tree = omni::ryml::core::parse(R"({"ports": {"api": 443}})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  std::map<std::string, int> into{{"api", 99}, {"stale", 1}};
  const auto result = omni::ryml::map_by_schema(
    &into, tree->crootref()["ports"]);

  const std::map<std::string, int> expected{{"api", 443}};
  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  EXPECT_EQ(expected, into);
}

TEST(mapping_diagnostics, reports_missing_fields_inside_sequence_records) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::vector<inplace_models::readings>>{},
    R"([{"first": 7}])");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  for (const auto &entry : result.diagnostics.issues) {
    EXPECT_EQ(omni::json::issue_code::missing_field, entry.reason);
    EXPECT_EQ(omni::json::value_kind::unknown, entry.actual);
    EXPECT_EQ(::ryml::NONE, entry.node_id);
    EXPECT_EQ("/0/second", omni::ryml::detail::render_path(entry.path));
  }
}

TEST(inplace_mapping, constrains_destination_mutability_and_tree_lifetime) {
  using binding_type = omni::ryml::map_by_schema_t<>;

  EXPECT_TRUE((omni::compat::is_invocable<const binding_type &,
    int *, const ::ryml::Tree &>::value));
  EXPECT_TRUE((omni::compat::is_invocable<const binding_type &,
    int *, ::ryml::ConstNodeRef>::value));
  EXPECT_FALSE((omni::compat::is_invocable<const binding_type &,
    const int *, const ::ryml::Tree &>::value));
  EXPECT_FALSE((omni::compat::is_invocable<const binding_type &,
    const int *, ::ryml::ConstNodeRef>::value));
  EXPECT_FALSE((omni::compat::is_invocable<const binding_type &,
    int *, ::ryml::Tree &&>::value));
}

TEST(inplace_deserialization, rejects_const_destinations) {
  using binding_type = omni::ryml::deserialize_t<>;

  EXPECT_TRUE((omni::compat::is_invocable<const binding_type &,
    int *, std::string>::value));
  EXPECT_FALSE((omni::compat::is_invocable<const binding_type &,
    const int *, std::string>::value));
}

TEST(inplace_deserialization, updates_an_enum) {
  auto into = inplace_models::mode::idle;
  const auto result = omni::ryml::deserialize(&into, R"("busy")");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  EXPECT_EQ(inplace_models::mode::busy, into);
}

TEST(inplace_deserialization, replaces_nested_sequences) {
  std::vector<std::vector<int>> into{{99}, {1, 2}};
  const auto result = omni::ryml::deserialize(&into, "[[7], []]");

  const std::vector<std::vector<int>> expected{{7}, {}};
  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  EXPECT_EQ(expected, into);
}

TEST(inplace_deserialization, resets_a_populated_optional_on_null) {
  omni::compat::optional<int> into{7};
  const auto result = omni::ryml::deserialize(&into, "null");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  EXPECT_FALSE(into);
}

TEST(inplace_deserialization, reports_duplicates_from_the_document) {
  std::map<std::string, int> into{{"api", 99}, {"stale", 1}};
  const auto result = serialization_test::deserialize(
    &into, R"({"api": 7, "api": 9})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (!result.diagnostics.issues.empty()) {
    EXPECT_EQ(omni::json::issue_code::duplicate_field,
      result.diagnostics.issues.front().reason);
    EXPECT_EQ("/api", omni::ryml::detail::render_path(
      result.diagnostics.issues.front().path));
  }
}

TEST(inplace_deserialization, returns_success_alongside_duplicate_warnings) {
  const auto load = serialization_test::deserialize.tdm_config(
    [](serialization_test::config options) {
      options.error_policy.duplicates = omni::json::duplicate_policy::allow
        | omni::json::duplicate_policy::warn;
      return options;
    });
  std::map<std::string, int> into{{"api", 99}, {"stale", 1}};
  const auto result = load(&into, R"({"api": 7, "api": 9})");

  const std::map<std::string, int> expected{{"api", 7}};
  EXPECT_TRUE(result.value);
  EXPECT_EQ(expected, into);
  EXPECT_FALSE(result.diagnostics.message.empty());
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (!result.diagnostics.issues.empty()) {
    EXPECT_TRUE(result.diagnostics.issues.front().warning);
    EXPECT_EQ(omni::json::issue_code::duplicate_field,
      result.diagnostics.issues.front().reason);
  }
}

TEST(inplace_deserialization, syntax_errors_preserve_existing_values) {
  const inplace_models::record initial{
    "old",
    {99, 100},
    {{"api", 99}, {"stale", 1}},
  };
  auto into = initial;
  const auto result = serialization_test::deserialize(&into, "{");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(initial.name, into.name);
  EXPECT_EQ(initial.samples, into.samples);
  EXPECT_EQ(initial.ports, into.ports);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (!result.diagnostics.issues.empty())
    EXPECT_EQ(omni::json::issue_code::parse_error,
      result.diagnostics.issues.front().reason);
}

TEST(inplace_deserialization, reports_mapping_errors) {
  inplace_models::readings into{7, 9};
  const auto result = omni::ryml::deserialize(
    &into, R"({"first": 42, "second": "bad"})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/second: expected integer, found scalar", result.diagnostics);
}

struct lvalue_renderer {
  std::string prefix;

  std::string operator()(const ::ryml::Tree &tree,
    omni::ryml::diagnostics report) & {
    return this->prefix + omni::ryml::render_diagnostics(tree, report);
  }
};

TEST(inplace_deserialization, preserves_lvalue_only_diagnostics_invocability) {
  using config_type = omni::ryml::config_t<lvalue_renderer>;
  using binding_type = omni::ryml::deserialize_t<config_type>;
  binding_type load{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
    /*base=*/{},
#endif
    /*config=*/{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*error_policy=*/{},
      /*transform_diagnostics=*/{"custom: "},
    },
  };

  int into = 7;
  const auto result = load(&into, "42");

  EXPECT_TRUE(result.value);
  EXPECT_EQ(42, into);
  EXPECT_EQ("custom: ", result.diagnostics);
  EXPECT_FALSE((omni::compat::is_invocable<const binding_type &,
    int *, std::string>::value));
  EXPECT_FALSE((omni::compat::is_invocable<binding_type &&,
    int *, std::string>::value));
}

struct consuming_renderer {
  std::unique_ptr<std::string> prefix;

  std::string operator()(const ::ryml::Tree &tree,
    omni::ryml::diagnostics report) && {
    return *this->prefix + omni::ryml::render_diagnostics(tree, report);
  }
};

TEST(inplace_deserialization, consumes_a_move_only_diagnostics_configuration) {
  using config_type = omni::ryml::config_t<consuming_renderer>;
  auto load = omni::ryml::deserialize_t<config_type>{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
    /*base=*/{},
#endif
    /*config=*/{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*error_policy=*/{},
      /*transform_diagnostics=*/{
        /*prefix=*/omni::compat::make_unique<std::string>("custom: "),
      },
    },
  };

  int into = 7;
  const auto result = std::move(load)(&into, "42");

  EXPECT_TRUE(result.value);
  EXPECT_EQ(42, into);
  EXPECT_EQ("custom: ", result.diagnostics);
}

} // namespace

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
