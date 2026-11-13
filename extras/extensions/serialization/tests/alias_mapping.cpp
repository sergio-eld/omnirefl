/** Experimental staging for tree alias normalization; the implementation and
 * rule representation remain local to this file while their interface evolves.
 * Path notation: JSONPath, RFC 9535 (https://www.rfc-editor.org/rfc/rfc9535).
 * This subset supports dot-member selectors and [*] wildcards. Member-name
 * aliases require a final member name; value aliases also support the root
 * and wildcard results. Other JSONPath selectors are not implemented.
 */
#include "json_match.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace alias_mapping {

struct alias_rule {
  std::string path; //< Canonical path selecting the member or value.
  std::vector<std::string> aliases; //< Alternative names for its final member.

  // Canonical scalar text -> accepted alternative spellings.
  std::map<std::string, std::vector<std::string>> value_aliases;
};

/// Parsed paths and inverse lookups prepared independently of input trees.
struct prepared_rule {
  // An empty step selects every child of an array or object.
  std::vector<std::string> path;
  std::vector<std::string> aliases;

  // Alternative spelling -> canonical scalar text.
  std::map<std::string, std::string> value_aliases;
};

/// Prepared rules in parent-before-child order, reusable across trees.
struct alias_lookup {
  std::vector<prepared_rule> rules;
};

namespace detail {

auto parse_path(const std::string &path)
  -> omni::compat::expected<std::vector<std::string>, std::string> {
  if (path.empty() || '$' != path.front())
    return omni::compat::unexpected<std::string>{
      "alias path must start with '$': " + path};
  std::vector<std::string> steps;
  std::size_t position = 1;
  while (position < path.size()) {
    if ('.' == path[position]) {
      const auto begin = ++position;
      position = path.find_first_of(".[", begin);
      if (std::string::npos == position)
        position = path.size();

      if (begin == position)
        return omni::compat::unexpected<std::string>{
          "empty member selector in alias path: " + path};

      steps.push_back(path.substr(begin, position - begin));
    } else if (0 == path.compare(position, 3, "[*]")) {
      steps.emplace_back();
      position += 3;
    } else {
      return omni::compat::unexpected<std::string>{
        "unsupported selector in alias path: " + path};
    }
  }

  return steps;
}

void apply_rule(const prepared_rule &rule,
  ::ryml::NodeRef *node,
  std::size_t step) {
  assert(node);
  if (rule.path.size() == step) {
    if (node->has_val() && !rule.value_aliases.empty()) {
      const auto text = omni::ryml::detail::to_string_view(node->val());
      const auto value = rule.value_aliases.find(
        std::string{text.data(), text.size()});
      if (rule.value_aliases.end() != value) {
        node->set_serialized(value->second);
        // Canonical value aliases remain strings, including "true" or "7".
        node->set_val_style(::ryml::VAL_DQUO);
      }
    }
    return;
  }

  const auto &name = rule.path[step];
  if (name.empty()) {
    if (node->is_seq() || node->is_map()) {
      for (auto child : node->children())
        apply_rule(rule, &child, step + 1);
    }
    return;
  }

  if (!node->is_map())
    return;

  for (auto child : node->children()) {
    if (step + 1 == rule.path.size()) {
      if (std::any_of(rule.aliases.begin(), rule.aliases.end(),
            [child](const std::string &alias) {
              return child.key() == c4::to_csubstr(alias);
            })) {
        // Copy the key so the tree can outlive the prepared rules.
        child.set_key_serialized(name);
      }
    }
    if (child.key() == c4::to_csubstr(name))
      apply_rule(rule, &child, step + 1);
  }
}

} // namespace detail

/// Prepare a rule's path and inverse value lookup, reporting invalid rules.
auto make_prepared_rule(alias_rule rule)
  -> omni::compat::expected<prepared_rule, std::string> {
  auto path = detail::parse_path(rule.path);
  if (!path)
    return omni::compat::unexpected<std::string>{std::move(path.error())};

  if (!rule.aliases.empty() && (path->empty() || path->back().empty()))
    return omni::compat::unexpected<std::string>{
      "member aliases require a final member name: " + rule.path};

  prepared_rule prepared{std::move(*path), std::move(rule.aliases), {}};
  for (const auto &value : rule.value_aliases) {
    for (const auto &alias : value.second) {
      const auto inserted = prepared.value_aliases.emplace(alias, value.first);
      if (!inserted.second && inserted.first->second != value.first)
        return omni::compat::unexpected<std::string>{
          "value alias '" + alias + "' maps to multiple values at " + rule.path};
    }
  }

  return prepared;
}

/// Prepare all rules, stopping at the first error, and order parents first.
auto make_alias_lookup(std::vector<alias_rule> rules)
  -> omni::compat::expected<alias_lookup, std::string> {
  alias_lookup lookup;
  lookup.rules.reserve(rules.size());
  for (auto &rule : rules) {
    auto prepared = make_prepared_rule(std::move(rule));
    if (!prepared)
      return omni::compat::unexpected<std::string>{std::move(prepared.error())};

    lookup.rules.push_back(std::move(*prepared));
  }

  // Descendant paths see canonical parent names regardless of rule order.
  std::stable_sort(lookup.rules.begin(), lookup.rules.end(),
    [](const prepared_rule &a, const prepared_rule &b) {
      return a.path.size() < b.path.size();
    });

  return lookup;
}

/// Normalize a tree using prepared paths and inverse value lookups.
::ryml::Tree map_aliases(const alias_lookup &lookup, ::ryml::Tree tree) {
  if (!tree.empty()) {
    auto root = tree.rootref();
    for (const auto &rule : lookup.rules)
      detail::apply_rule(rule, &root, /*step=*/0);
  }
  return tree;
}

} // namespace alias_mapping

struct normalization_case {
  std::string name; //< Searchable parameter name for running one case.
  std::string source; //< JSON or YAML before normalization.
  std::vector<alias_mapping::alias_rule> rules;
  std::string expected; //< Expected tree expressed as readable JSON.
};

const std::vector<normalization_case> cases{
  {
    "empty_rules",
    R"({"timeout": 25})",
    {},
    R"({"timeout": 25})",
  },
  {
    "empty_aliases",
    R"({"timeout": 25})",
    {{"$.timeout_ms", {}, {}}},
    R"({"timeout": 25})",
  },
  {
    "empty_object_root",
    "{}",
    {{"$.timeout_ms", {"timeout"}, {}}},
    "{}",
  },
  {
    "empty_sequence_root",
    "[]",
    {{"$[*].timeout_ms", {"timeout"}, {}}},
    "[]",
  },
  {
    "preserves_canonical_name",
    R"({"timeout_ms": 25})",
    {{"$.timeout_ms", {"timeout"}, {}}},
    R"({"timeout_ms": 25})",
  },
  {
    "renames_member",
    R"({"timeout": 25})",
    {{"$.timeout_ms", {"timeout"}, {}}},
    R"({"timeout_ms": 25})",
  },
  {
    "accepts_another_alias",
    R"({"request_timeout": 25})",
    {{"$.timeout_ms", {"timeout", "request_timeout"}, {}}},
    R"({"timeout_ms": 25})",
  },
  {
    "normalizes_several_members",
    R"({"timeout": 25, "retry_count": 3, "enabled": true})",
    {
      {"$.timeout_ms", {"timeout"}, {}},
      {"$.retries", {"retry_count"}, {}},
    },
    R"({"timeout_ms": 25, "retries": 3, "enabled": true})",
  },
  {
    "preserves_unmatched_members",
    R"({"retries": 3, "label": "timeout"})",
    {{"$.timeout_ms", {"timeout"}, {}}},
    R"({"retries": 3, "label": "timeout"})",
  },
  {
    "nested_path_stays_scoped",
    R"({"primary": {"timeout": 25}, "backup": {"timeout": 50}})",
    {{"$.primary.timeout_ms", {"timeout"}, {}}},
    R"({"primary": {"timeout_ms": 25}, "backup": {"timeout": 50}})",
  },
  {
    "alias_names_are_literal",
    R"({"old.timeout": 25})",
    {{"$.timeout_ms", {"old.timeout"}, {}}},
    R"({"timeout_ms": 25})",
  },
  {
    "parents_precede_children",
    R"({"network": {"endpoint": {"request_timeout": 25}}})",
    {
      {"$.transport.primary.timeout_ms", {"request_timeout"}, {}},
      {"$.transport.primary", {"endpoint"}, {}},
      {"$.transport", {"network"}, {}},
    },
    R"({"transport": {"primary": {"timeout_ms": 25}}})",
  },
  {
    "normalizes_sibling_paths",
    R"({"primary": {"timeout": 25}, "backup": {"timeout": 50}})",
    {
      {"$.primary.timeout_ms", {"timeout"}, {}},
      {"$.backup.retry_delay_ms", {"timeout"}, {}},
    },
    R"({"primary": {"timeout_ms": 25}, "backup": {"retry_delay_ms": 50}})",
  },
  {
    "normalizes_each_sequence_element",
    R"({"replicas": [{"timeout": 25}, {"request_timeout": 50}]})",
    {{"$.replicas[*].timeout_ms", {"timeout", "request_timeout"}, {}}},
    R"({"replicas": [{"timeout_ms": 25}, {"timeout_ms": 50}]})",
  },
  {
    "normalizes_sequence_parent_before_elements",
    R"({"backups": [{"timeout": 25}, {"timeout": 50}]})",
    {
      {"$.replicas[*].timeout_ms", {"timeout"}, {}},
      {"$.replicas", {"backups"}, {}},
    },
    R"({"replicas": [{"timeout_ms": 25}, {"timeout_ms": 50}]})",
  },
  {
    "preserves_other_sequence_elements",
    R"({"replicas": [{"timeout": 25}, null, 7, "timeout", {}]})",
    {{"$.replicas[*].timeout_ms", {"timeout"}, {}}},
    R"({"replicas": [{"timeout_ms": 25}, null, 7, "timeout", {}]})",
  },
  {
    "nested_sequences",
    R"({"groups": [[{"timeout": 25}], [], [{"timeout": 50}]]})",
    {{"$.groups[*][*].timeout_ms", {"timeout"}, {}}},
    R"({"groups": [[{"timeout_ms": 25}], [], [{"timeout_ms": 50}]]})",
  },
  {
    "sequence_root",
    R"([{"timeout": 25}, {"timeout": 50}])",
    {{"$[*].timeout_ms", {"timeout"}, {}}},
    R"([{"timeout_ms": 25}, {"timeout_ms": 50}])",
  },
  {
    "wildcard_selects_object_values",
    R"({"regions": {"east": {"timeout": 25}, "west": {"timeout": 50}}})",
    {{"$.regions[*].timeout_ms", {"timeout"}, {}}},
    R"({"regions": {"east": {"timeout_ms": 25}, "west": {"timeout_ms": 50}}})",
  },
  {
    "missing_parent",
    R"({"backup": {"timeout": 25}})",
    {{"$.primary.timeout_ms", {"timeout"}, {}}},
    R"({"backup": {"timeout": 25}})",
  },
  {
    "scalar_parent",
    R"({"primary": 25})",
    {{"$.primary.timeout_ms", {"timeout"}, {}}},
    R"({"primary": 25})",
  },
  {
    "null_parent",
    R"({"primary": null})",
    {{"$.primary.timeout_ms", {"timeout"}, {}}},
    R"({"primary": null})",
  },
  {
    "sequence_parent_requires_wildcard",
    R"({"primary": [{"timeout": 25}]})",
    {{"$.primary.timeout_ms", {"timeout"}, {}}},
    R"({"primary": [{"timeout": 25}]})",
  },
  {
    "empty_sequence",
    R"({"replicas": []})",
    {{"$.replicas[*].timeout_ms", {"timeout"}, {}}},
    R"({"replicas": []})",
  },
  {
    "scalar_root",
    "25",
    {{"$.timeout_ms", {"timeout"}, {}}},
    "25",
  },
  {
    "null_root",
    "null",
    {{"$.timeout_ms", {"timeout"}, {}}},
    "null",
  },
  {
    "preserves_member_values",
    R"({"legacy": {"enabled": true, "label": "timeout", "values": [1, null]}})",
    {{"$.primary", {"legacy"}, {}}},
    R"({"primary": {"enabled": true, "label": "timeout", "values": [1, null]}})",
  },
  {
    "owns_long_canonical_name",
    R"({"connect_timeout": 25})",
    {
      {
        "$.connection_establishment_timeout_in_milliseconds",
        {"connect_timeout"},
        {},
      },
    },
    R"({"connection_establishment_timeout_in_milliseconds": 25})",
  },
  {
    "yaml_source",
    "primary:\n  request_timeout: 25\n",
    {{"$.primary.timeout_ms", {"request_timeout"}, {}}},
    R"({"primary": {"timeout_ms": 25}})",
  },
  {
    "preserves_canonical_value",
    R"({"state": "ready"})",
    {{"$.state", {}, {{"ready", {"online"}}}}},
    R"({"state": "ready"})",
  },
  {
    "preserves_unmatched_value",
    R"({"state": "degraded"})",
    {{"$.state", {}, {{"ready", {"online"}}}}},
    R"({"state": "degraded"})",
  },
  {
    "normalizes_value",
    R"({"state": "online"})",
    {{"$.state", {}, {{"ready", {"online"}}}}},
    R"({"state": "ready"})",
  },
  {
    "accepts_another_value_alias",
    R"({"state": "available"})",
    {{"$.state", {}, {{"ready", {"online", "available"}}}}},
    R"({"state": "ready"})",
  },
  {
    "selects_canonical_value",
    R"({"state": "offline"})",
    {
      {
        "$.state",
        {},
        {
          {"ready", {"online", "available"}},
          {"stopped", {"offline", "disabled"}},
        },
      },
    },
    R"({"state": "stopped"})",
  },
  {
    "normalizes_member_and_value",
    R"({"status": "online"})",
    {{"$.state", {"status"}, {{"ready", {"online"}}}}},
    R"({"state": "ready"})",
  },
  {
    "renames_member_with_unmatched_value",
    R"({"status": "degraded"})",
    {{"$.state", {"status"}, {{"ready", {"online"}}}}},
    R"({"state": "degraded"})",
  },
  {
    "value_mapping_stays_scoped",
    R"({"primary": {"state": "online"}, "backup": {"state": "online"}})",
    {{"$.primary.state", {}, {{"ready", {"online"}}}}},
    R"({"primary": {"state": "ready"}, "backup": {"state": "online"}})",
  },
  {
    "parent_rename_precedes_member_and_value",
    R"({"main": {"status": "online"}})",
    {
      {"$.primary.state", {"status"}, {{"ready", {"online"}}}},
      {"$.primary", {"main"}, {}},
    },
    R"({"primary": {"state": "ready"}})",
  },
  {
    "normalizes_values_in_sequence_records",
    R"({"replicas": [{"status": "online"}, {"state": "available"}]})",
    {
      {
        "$.replicas[*].state",
        {"status"},
        {{"ready", {"online", "available"}}},
      },
    },
    R"({"replicas": [{"state": "ready"}, {"state": "ready"}]})",
  },
  {
    "normalizes_sequence_values",
    R"({"states": ["online", "available", "degraded"]})",
    {{"$.states[*]", {}, {{"ready", {"online", "available"}}}}},
    R"({"states": ["ready", "ready", "degraded"]})",
  },
  {
    "normalizes_sequence_root_values",
    R"(["online", "available"])",
    {{"$[*]", {}, {{"ready", {"online", "available"}}}}},
    R"(["ready", "ready"])",
  },
  {
    "normalizes_root_value",
    R"("online")",
    {{"$", {}, {{"ready", {"online"}}}}},
    R"("ready")",
  },
  {
    "normalizes_object_values",
    R"({"states": {"primary": "online", "backup": "offline"}})",
    {
      {
        "$.states[*]",
        {},
        {
          {"ready", {"online"}},
          {"stopped", {"offline"}},
        },
      },
    },
    R"({"states": {"primary": "ready", "backup": "stopped"}})",
  },
  {
    "normalizes_nested_sequence_values",
    R"({"states": [["online"], [], ["available", "degraded"]]})",
    {{"$.states[*][*]", {}, {{"ready", {"online", "available"}}}}},
    R"({"states": [["ready"], [], ["ready", "degraded"]]})",
  },
  {
    "value_aliases_are_literal",
    R"({"state": "legacy.online"})",
    {{"$.state", {}, {{"ready", {"legacy.online"}}}}},
    R"({"state": "ready"})",
  },
  {
    "empty_value_alias",
    R"({"state": ""})",
    {{"$.state", {}, {{"ready", {""}}}}},
    R"({"state": "ready"})",
  },
  {
    "empty_canonical_value",
    R"({"state": "unset"})",
    {{"$.state", {}, {{"", {"unset"}}}}},
    R"({"state": ""})",
  },
  {
    "canonical_values_remain_strings",
    R"(["legacy_number", "legacy_boolean", "legacy_null"])",
    {
      {
        "$[*]",
        {},
        {
          {"7", {"legacy_number"}},
          {"true", {"legacy_boolean"}},
          {"null", {"legacy_null"}},
        },
      },
    },
    R"(["7", "true", "null"])",
  },
  {
    "value_mapping_preserves_unmatched_scalars",
    R"({"states": [7, true, false, null, "online"]})",
    {{"$.states[*]", {}, {{"ready", {"online"}}}}},
    R"({"states": [7, true, false, null, "ready"]})",
  },
  {
    "value_mapping_requires_wildcard_for_sequences",
    R"({"states": ["online", "available"]})",
    {{"$.states", {}, {{"ready", {"online", "available"}}}}},
    R"({"states": ["online", "available"]})",
  },
  {
    "value_mapping_preserves_objects",
    R"({"state": {"raw": "online"}})",
    {{"$.state", {}, {{"ready", {"online"}}}}},
    R"({"state": {"raw": "online"}})",
  },
  {
    "renames_container_without_mapping_its_values",
    R"({"status": {"raw": "online"}})",
    {{"$.state", {"status"}, {{"ready", {"online"}}}}},
    R"({"state": {"raw": "online"}})",
  },
  {
    "yaml_plain_value",
    "status: online\n",
    {{"$.state", {"status"}, {{"ready", {"online"}}}}},
    R"({"state": "ready"})",
  },
};

class alias_normalization: public testing::TestWithParam<normalization_case> {};

TEST_P(alias_normalization, maps_to_expected_tree) {
  const auto &c = GetParam();
  const auto lookup = alias_mapping::make_alias_lookup(c.rules);
  EXPECT_TRUE(lookup) << (lookup ? std::string{} : lookup.error());
  if (!lookup)
    return;

  auto tree = omni::ryml::core::parse(c.source);
  EXPECT_TRUE(tree) << (tree ? std::string{} : tree.error());
  if (!tree)
    return;

  const auto normalized = alias_mapping::map_aliases(*lookup, std::move(*tree));

  EXPECT_TRUE(serialization_test::matches_node(
    c.expected, normalized.crootref()));
}

INSTANTIATE_TEST_SUITE_P(documents,
  alias_normalization,
  testing::ValuesIn(cases),
  [](const testing::TestParamInfo<normalization_case> &c) {
    return c.param.name;
  });

struct invalid_rules_case {
  std::string name;
  std::vector<alias_mapping::alias_rule> rules;
  std::string error;
};

const std::vector<invalid_rules_case> invalid_rules{
  {
    "empty_path",
    {{"", {}, {}}},
    "alias path must start with '$': ",
  },
  {
    "missing_root",
    {{"state", {}, {}}},
    "alias path must start with '$': state",
  },
  {
    "empty_member",
    {{"$.", {}, {}}},
    "empty member selector in alias path: $.",
  },
  {
    "unsupported_index",
    {{"$.states[0]", {}, {}}},
    "unsupported selector in alias path: $.states[0]",
  },
  {
    "member_alias_at_root",
    {{"$", {"state"}, {}}},
    "member aliases require a final member name: $",
  },
  {
    "member_alias_at_wildcard",
    {{"$[*]", {"state"}, {}}},
    "member aliases require a final member name: $[*]",
  },
  {
    "ambiguous_value_alias",
    {
      {
        "$.state",
        {},
        {
          {"ready", {"online"}},
          {"stopped", {"online"}},
        },
      },
    },
    "value alias 'online' maps to multiple values at $.state",
  },
  {
    "first_invalid_rule",
    {
      {"$.state", {}, {{"ready", {"online"}}}},
      {"state", {}, {}},
      {"$.", {}, {}},
    },
    "alias path must start with '$': state",
  },
};

class alias_preparation: public testing::TestWithParam<invalid_rules_case> {};

TEST_P(alias_preparation, reports_invalid_rule) {
  const auto &c = GetParam();
  const auto lookup = alias_mapping::make_alias_lookup(c.rules);

  EXPECT_FALSE(lookup);
  if (!lookup) {
    EXPECT_EQ(c.error, lookup.error());
  }
}

INSTANTIATE_TEST_SUITE_P(rules,
  alias_preparation,
  testing::ValuesIn(invalid_rules),
  [](const testing::TestParamInfo<invalid_rules_case> &c) {
    return c.param.name;
  });

} // namespace
