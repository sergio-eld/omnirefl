#include "json_match.hpp"

#include <string>
#include <tuple>
#include <vector>

#if defined(__GNUC__) && !defined(__clang__)
// GCC warns about GTest's internal conditionals in unbraced assertion guards.
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdangling-else"
#endif

namespace {

using string_view = omni::compat::string_view;

namespace enum_alias_models {

enum class state { idle, active, null };

struct service {
  state mode;
  std::vector<state> history;
};

// C++11 callable: prepare the default alias dictionary once.
struct state_names {
  omni::compat::optional<string_view> operator()(string_view alias) const {
    static const auto names = omni::ryml::alias_dict(
      std::make_tuple("idle", "available", ""),
      std::make_tuple("active", "running", "working", "7", "true"),
      std::make_tuple("null", "nil"),
      std::make_tuple("absent", "obsolete"));

    return names(alias);
  }
};

} // namespace enum_alias_models
} // namespace

namespace omni {
namespace ryml {
namespace cp {

template <>
struct to_type<enum_alias_models::state>:
    reflected_scope::to_enum_t<enum_alias_models::state_names> {};

} // namespace cp
} // namespace ryml
} // namespace omni

namespace {

using enum_alias_models::state;
using omni::json::issue_code;

TEST(to_enum_aliases, dictionary_replaces_the_previous_resolver) {
  const auto original =
    omni::ryml::reflected_scope::to_enum.with_value_by_alias(
      omni::ryml::alias_dict(std::make_tuple("idle", "available")));

  const auto replacement = original.with_value_by_alias(
    omni::ryml::alias_dict(std::make_tuple("active", "working")));

  const auto idle = original.value_by_alias("available");
  EXPECT_TRUE(idle);
  if (idle)
    EXPECT_EQ("idle", *idle);

  EXPECT_FALSE(original.value_by_alias("working"));

  const auto active = replacement.value_by_alias("working");
  EXPECT_TRUE(active);
  if (active)
    EXPECT_EQ("active", *active);

  EXPECT_FALSE(replacement.value_by_alias("available"));
}

// C++11 lambda equivalent: apply a local mapper inside the reflected scope.
// The explicit return type delays the body until metadata is generated.
template <typename Compositor>
struct cpp11_map_with_aliases {
  Compositor compositor;
  ::ryml::ConstNodeRef node;
  const omni::ryml::detail::mapping_context &context;

  template <typename _E>
  auto operator()(omni::enum_binding_t<_E> enumeration) const
    -> omni::ryml::with_diagnostics<omni::compat::monostate,
      omni::ryml::diagnostics<false>> {
    return this->compositor(this->node, &enumeration.value(), this->context);
  }
};

TEST(to_enum_aliases, declared_names_bypass_the_resolver) {
  std::size_t calls = 0;
  const auto compositor =
    omni::ryml::reflected_scope::to_enum.with_value_by_alias(
      [&calls](string_view) -> omni::compat::optional<string_view> {
        ++calls;
        return omni::compat::nullopt;
      });

  const auto tree = omni::ryml::core::parse(R"("active")");
  EXPECT_TRUE(tree);
  if (tree) {
    const omni::ryml::config options{};
    const omni::ryml::detail::mapping_context context{
      /*options=*/options,
      /*path=*/{},
      /*error_count=*/0,
      /*warning_count=*/0,
      /*borrow_strings=*/false,
    };

    state into = state::idle;
    const auto result = omni::reflected_call(
      cpp11_map_with_aliases<decltype(compositor)>{
        /*compositor=*/compositor,
        /*node=*/tree->crootref(),
        /*context=*/context,
      },

      into);

    EXPECT_TRUE(result.value);
    EXPECT_EQ(state::active, into);
    EXPECT_TRUE(result.diagnostics.issues.empty());
  }

  EXPECT_EQ(0U, calls);
}

struct enum_alias_case {
  std::string name;
  std::string document;
  omni::compat::expected<state, issue_code> expected;
};

const enum_alias_case enum_alias_cases[]{
  {"json_declared", R"("active")", state::active},
  {"yaml_declared", "idle", state::idle},
  {"json_alias", R"("running")", state::active},
  {"yaml_alias", "working", state::active},
  {"another_alias_group", R"("available")", state::idle},
  {"empty_alias", R"("")", state::idle},
  {"declared_null_name", R"("null")", state::null},
  {"null_name_alias", "nil", state::null},
  {"quoted_number_alias", R"("7")", state::active},
  {"quoted_boolean_alias", R"("true")", state::active},
  {"tagged_number_alias", "!!str 7", state::active},
  {
    "integer_tag_is_not_an_alias",
    "!!int 7",
    omni::compat::unexpected<issue_code>{issue_code::unexpected_kind},
  },
  {
    "unknown_name",
    R"("missing")",
    omni::compat::unexpected<issue_code>{issue_code::invalid_scalar},
  },
  {
    "unknown_alias_target",
    R"("obsolete")",
    omni::compat::unexpected<issue_code>{issue_code::invalid_scalar},
  },
  {
    "case_sensitive",
    R"("WORKING")",
    omni::compat::unexpected<issue_code>{issue_code::invalid_scalar},
  },
  {
    "plain_number_is_not_an_alias",
    "7",
    omni::compat::unexpected<issue_code>{issue_code::unexpected_kind},
  },
  {
    "plain_boolean_is_not_an_alias",
    "true",
    omni::compat::unexpected<issue_code>{issue_code::unexpected_kind},
  },
  {
    "plain_null_is_not_a_name",
    "null",
    omni::compat::unexpected<issue_code>{issue_code::unexpected_kind},
  },
  {
    "mapping_is_not_a_name",
    "{}",
    omni::compat::unexpected<issue_code>{issue_code::unexpected_kind},
  },
  {
    "sequence_is_not_a_name",
    "[]",
    omni::compat::unexpected<issue_code>{issue_code::unexpected_kind},
  },
};

class enum_aliases: public testing::TestWithParam<enum_alias_case> {};

TEST_P(enum_aliases, maps_strings_and_preserves_kind_validation) {
  const auto &c = GetParam();
  const auto result = omni::ryml::deserialize(
    omni::type_t<state>{}, c.document);

  EXPECT_EQ(bool(c.expected), bool(result.value));
  if (c.expected) {
    if (result.value)
      EXPECT_EQ(*c.expected, *result.value);

    EXPECT_TRUE(result.diagnostics.issues.empty());
  } else {
    const auto &issues = result.diagnostics.issues;
    EXPECT_EQ(1U, issues.size());
    if (!issues.empty()) {
      EXPECT_EQ(c.expected.error(), issues.front().reason);
      EXPECT_EQ(omni::json::value_kind::string, issues.front().expected);
      EXPECT_TRUE(issues.front().path.empty());
      EXPECT_FALSE(issues.front().warning);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(documents,
  enum_aliases,
  testing::ValuesIn(enum_alias_cases),
  [](const testing::TestParamInfo<enum_alias_case> &info) {
    return info.param.name;
  });

TEST(enum_aliases, maps_nested_members_and_sequence_elements) {
  const auto result = omni::ryml::deserialize(
    omni::type_t<enum_alias_models::service>{},
    R"({"mode":"working","history":["available","running","nil"]})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_EQ(state::active, result.value->mode);
    EXPECT_EQ((std::vector<state>{state::idle, state::active, state::null}),
      result.value->history);
  }
}

TEST(enum_aliases, retains_the_original_value_and_nested_path_on_failure) {
  const auto result = omni::ryml::deserialize(
    omni::type_t<enum_alias_models::service>{},
    R"({"mode":"obsolete","history":[]})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (!result.diagnostics.issues.empty()) {
    const auto &issue = result.diagnostics.issues.front();
    EXPECT_EQ(issue_code::invalid_scalar, issue.reason);
    EXPECT_EQ("/mode", omni::ryml::detail::render_path(issue.path));
    EXPECT_EQ("obsolete", omni::ryml::detail::to_string(issue.value));
  }
}

TEST(enum_aliases, serializes_a_mapped_alias_as_a_declared_name) {
  const auto result = omni::ryml::deserialize(
    omni::type_t<state>{}, R"("working")");

  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_TRUE(serialization_test::matches_json(
      R"("active")", omni::ryml::as_json(*result.value)));

    EXPECT_TRUE(serialization_test::matches_json(
      R"("active")", omni::ryml::as_yaml(*result.value)));
  }
}

} // namespace

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
