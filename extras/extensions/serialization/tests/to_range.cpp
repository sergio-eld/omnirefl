#include "json_match.hpp"

#include <deque>
#include <list>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if defined(__GNUC__) && !defined(__clang__)
// GCC warns about GTest's internal conditionals in unbraced assertion guards.
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdangling-else"
#endif

namespace {
namespace range_models {

struct reading { int value; };

// Deliberately expose iteration through ADL rather than member begin/end.
template <typename T, typename Storage = std::vector<T>>
struct wrapped_values {
  using value_type = T;
  Storage values;

  friend auto begin(wrapped_values &r) -> typename decltype(values)::iterator {
    return r.values.begin();
  }

  friend auto end(wrapped_values &r) -> typename decltype(values)::iterator {
    return r.values.end();
  }

  friend auto begin(const wrapped_values &r)
    -> typename decltype(values)::const_iterator {
    return r.values.begin();
  }

  friend auto end(const wrapped_values &r)
    -> typename decltype(values)::const_iterator {
    return r.values.end();
  }
};

template <typename Key, typename Value>
struct wrapped_map {
  using key_type = Key;
  using mapped_type = Value;
  std::map<Key, Value> entries;
  using value_type = typename decltype(entries)::value_type;

  friend auto begin(wrapped_map &r) -> typename decltype(entries)::iterator {
    return r.entries.begin();
  }

  friend auto end(wrapped_map &r) -> typename decltype(entries)::iterator {
    return r.entries.end();
  }

  friend auto begin(const wrapped_map &r)
    -> typename decltype(entries)::const_iterator {
    return r.entries.begin();
  }

  friend auto end(const wrapped_map &r)
    -> typename decltype(entries)::const_iterator {
    return r.entries.end();
  }
};

struct keyed_value {
  int id;
  std::string label;

  bool operator<(const keyed_value &other) const { return id < other.id; }
};

} // namespace range_models
} // namespace

#if defined(OMNI_TOOL_RUN)
// Skip wrapper metadata while retaining value_type/mapped_type discovery.
// Concrete arguments identify the primary templates for these skip requests.
OMNI_SKIP_METADATA(range_models::wrapped_values<int>,
  range_models::wrapped_map<std::string, int>);
#endif

namespace omni {
namespace traits {
namespace cp {

template <typename T, typename Storage>
struct clear<range_models::wrapped_values<T, Storage>> {
  void operator()(range_models::wrapped_values<T, Storage> *into) const {
    adapt::clear(&into->values);
  }
};

template <typename T, typename Storage>
struct is_unique_key<range_models::wrapped_values<T, Storage>>:
    traits::is_unique_key<Storage> {};

// Unique wrappers intentionally provide only try_emplace and replace.
template <typename T, typename Storage>
struct emplace<range_models::wrapped_values<T, Storage>,
  compat::enable_if_t<!traits::is_unique_key<Storage>::value>> {
  void operator()(range_models::wrapped_values<T, Storage> *into,
    T &&value) const {
    adapt::emplace(&into->values, std::move(value));
  }
};

template <typename T, typename Storage>
struct try_emplace<range_models::wrapped_values<T, Storage>,
  compat::enable_if_t<traits::is_unique_key<Storage>::value>> {
  auto operator()(range_models::wrapped_values<T, Storage> *into,
    T &&value) const
    -> decltype(adapt::try_emplace(&into->values, std::move(value))) {
    return adapt::try_emplace(&into->values, std::move(value));
  }
};

template <typename T, typename Storage>
struct replace<range_models::wrapped_values<T, Storage>,
  compat::enable_if_t<traits::is_unique_key<Storage>::value>> {
  template <typename Position>
  void operator()(range_models::wrapped_values<T, Storage> *into,
    Position position, T &&value) const {
    adapt::replace(&into->values, position, std::move(value));
  }
};

template <typename Key, typename Value>
struct is_unique_key<range_models::wrapped_map<Key, Value>>: std::true_type {};

template <typename Key, typename Value>
struct clear<range_models::wrapped_map<Key, Value>> {
  void operator()(range_models::wrapped_map<Key, Value> *into) const {
    into->entries.clear();
  }
};

template <typename Key, typename Value>
struct try_emplace<range_models::wrapped_map<Key, Value>> {
  auto operator()(range_models::wrapped_map<Key, Value> *into,
    Key &&key, Value &&value) const
    -> decltype(adapt::try_emplace(&into->entries,
      std::move(key), std::move(value))) {
    return adapt::try_emplace(&into->entries, std::move(key), std::move(value));
  }
};

template <typename Key, typename Value>
struct replace<range_models::wrapped_map<Key, Value>> {
  template <typename Position>
  void operator()(range_models::wrapped_map<Key, Value> *into,
    Position position, Key &&key, Value &&value) const {
    adapt::replace(&into->entries, position, std::move(key), std::move(value));
  }
};

} // namespace cp
} // namespace traits
} // namespace omni

namespace {

using omni::json::duplicate_policy;
using omni::json::issue_code;

template <typename T>
class sequence_ranges: public testing::Test {};

using sequences = testing::Types<std::vector<int>, std::deque<int>,
  std::list<int>, range_models::wrapped_values<int>>;

TYPED_TEST_SUITE(sequence_ranges, sequences);

TYPED_TEST(sequence_ranges, retains_order_and_repeated_elements) {
  const auto result = serialization_test::deserialize(
    omni::type_t<TypeParam>{}, "[7, 4, 7]");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    using std::begin;
    using std::end;
    const std::vector<int> actual{begin(*result.value), end(*result.value)};
    EXPECT_EQ((std::vector<int>{7, 4, 7}), actual);
    EXPECT_TRUE(serialization_test::matches_json(
      "[7, 4, 7]", omni::ryml::as_json(*result.value)));
  }
}

template <typename T>
class object_ranges: public testing::Test {};

using objects = testing::Types<std::map<std::string, int>,
  std::unordered_map<std::string, int>,
  range_models::wrapped_map<std::string, int>>;

TYPED_TEST_SUITE(object_ranges, objects);

TYPED_TEST(object_ranges, maps_string_keys_and_values) {
  const std::string document = R"({"first":7,"second":42})";
  const auto result = serialization_test::deserialize(
    omni::type_t<TypeParam>{}, document);

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    using std::begin;
    using std::end;
    const std::map<std::string, int> actual{
      begin(*result.value), end(*result.value),
    };

    EXPECT_EQ((std::map<std::string, int>{{"first", 7}, {"second", 42}}),
      actual);

    EXPECT_TRUE(serialization_test::matches_json(
      document, omni::ryml::as_json(*result.value)));

    EXPECT_TRUE(serialization_test::matches_json(
      document, omni::ryml::as_yaml(*result.value)));
  }
}

struct map_duplicate_case {
  std::string name;
  duplicate_policy policy;
  std::string document;
  omni::compat::expected<int, issue_code> expected;
  bool warning;
};

const map_duplicate_case map_duplicate_cases[]{
  {"reject", duplicate_policy::reject, R"({"port":7,"port":8})",
    omni::compat::unexpected<issue_code>{issue_code::duplicate_field}, false},
  {"first", duplicate_policy::allow, R"({"port":7,"port":8})", 7, false},
  {"last", duplicate_policy::last, R"({"port":7,"port":8})", 8, false},
  {"first_warn", duplicate_policy::allow | duplicate_policy::warn,
    R"({"port":7,"port":8})", 7, true},
  {"last_warn", duplicate_policy::last | duplicate_policy::warn,
    R"({"port":7,"port":8})", 8, true},
  {"ignore_invalid_last", duplicate_policy::allow,
    R"({"port":7,"port":"bad"})", 7, false},
  {"ignore_invalid_first", duplicate_policy::last,
    R"({"port":"bad","port":8})", 8, false},
  {"invalid_selected_last", duplicate_policy::last,
    R"({"port":7,"port":"bad"})",
    omni::compat::unexpected<issue_code>{issue_code::invalid_scalar}, false},
};

class unique_map_duplicates:
    public testing::TestWithParam<map_duplicate_case> {};

TEST_P(unique_map_duplicates, selects_occurrences_before_converting_values) {
  const auto &c = GetParam();
  const auto load = serialization_test::deserialize.transform_config(
    [&c](serialization_test::config options) {
      options.error_policy.duplicates = c.policy;
      return options;
    });

  const auto result = load(
    omni::type_t<std::map<std::string, int>>{}, c.document);

  EXPECT_EQ(bool(c.expected), bool(result.value));
  if (c.expected && result.value) {
    const std::map<std::string, int> expected{{"port", *c.expected}};
    EXPECT_EQ(expected, *result.value);
  }

  EXPECT_EQ(c.expected ? std::size_t(c.warning) : 1U,
    result.diagnostics.issues.size());

  for (const auto &issue : result.diagnostics.issues) {
    EXPECT_EQ(c.expected ? issue_code::duplicate_field : c.expected.error(),
      issue.reason);

    EXPECT_EQ("/port", omni::ryml::detail::render_path(issue.path));
    EXPECT_EQ(c.warning, issue.warning);
  }
}

INSTANTIATE_TEST_SUITE_P(policies,
  unique_map_duplicates,
  testing::ValuesIn(map_duplicate_cases),
  [](const testing::TestParamInfo<map_duplicate_case> &info) {
    return info.param.name;
  });

template <typename T>
class non_unique_objects: public testing::Test {};

using multi_objects = testing::Types<std::multimap<std::string, int>,
  std::unordered_multimap<std::string, int>>;

TYPED_TEST_SUITE(non_unique_objects, multi_objects);

TYPED_TEST(non_unique_objects, retains_every_repeated_key) {
  const auto result = serialization_test::deserialize(
    omni::type_t<TypeParam>{}, R"({"port":7,"port":8})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    const auto entries = result.value->equal_range("port");
    std::multiset<int> values;
    for (auto current = entries.first; current != entries.second; ++current)
      values.emplace(current->second);

    EXPECT_EQ((std::multiset<int>{7, 8}), values);
    EXPECT_EQ(2U, result.value->size());
    const auto restored = serialization_test::deserialize(
      omni::type_t<TypeParam>{}, omni::ryml::as_json(*result.value));

    EXPECT_TRUE(restored.value);
    if (restored.value)
      EXPECT_EQ(*result.value, *restored.value);
  }
}

struct unique_element_case {
  std::string name;
  duplicate_policy policy;
  omni::compat::optional<std::string> label;
  bool warning;
};

const unique_element_case unique_element_cases[]{
  {"reject", duplicate_policy::reject, omni::compat::nullopt, false},
  {"first", duplicate_policy::allow, std::string{"first"}, false},
  {"last", duplicate_policy::last, std::string{"last"}, false},
  {"first_warn", duplicate_policy::allow | duplicate_policy::warn,
    std::string{"first"}, true},
  {"last_warn", duplicate_policy::last | duplicate_policy::warn,
    std::string{"last"}, true},
};

class unique_elements: public testing::TestWithParam<unique_element_case> {};

TEST_P(unique_elements, applies_policy_without_consuming_a_rejected_value) {
  const auto &c = GetParam();
  const auto load = serialization_test::deserialize.transform_config(
    [&c](serialization_test::config options) {
      options.error_policy.duplicates = c.policy;
      return options;
    });

  const auto result = load(omni::type_t<std::set<range_models::keyed_value>>{},
    R"([{"id":7,"label":"first"},{"id":7,"label":"last"}])");

  EXPECT_EQ(bool(c.label), bool(result.value));
  if (c.label && result.value) {
    EXPECT_EQ(1U, result.value->size());
    if (!result.value->empty()) {
      EXPECT_EQ(7, result.value->begin()->id);
      EXPECT_EQ(*c.label, result.value->begin()->label);
    }
  }

  EXPECT_EQ(c.label ? std::size_t(c.warning) : 1U,
    result.diagnostics.issues.size());

  for (const auto &issue : result.diagnostics.issues) {
    EXPECT_EQ(issue_code::duplicate_element, issue.reason);
    EXPECT_EQ("/1", omni::ryml::detail::render_path(issue.path));
    EXPECT_EQ(c.warning, issue.warning);
  }
}

INSTANTIATE_TEST_SUITE_P(policies,
  unique_elements,
  testing::ValuesIn(unique_element_cases),
  [](const testing::TestParamInfo<unique_element_case> &info) {
    return info.param.name;
  });

TEST(range_mapping, discovers_skipped_wrappers_record_elements) {
  const auto result = serialization_test::deserialize(
    omni::type_t<range_models::wrapped_values<range_models::reading>>{},
    R"([{"value":7},{"value":42}])");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    std::vector<int> values;
    for (const auto &reading : result.value->values)
      values.push_back(reading.value);

    EXPECT_EQ((std::vector<int>{7, 42}), values);
    EXPECT_TRUE(serialization_test::matches_json(
      R"([{"value":7},{"value":42}])", omni::ryml::as_json(*result.value)));
  }
}

TEST(range_mapping, discovers_skipped_wrappers_mapped_record_type) {
  const auto result = serialization_test::deserialize(
    omni::type_t<range_models::wrapped_map<std::string,
      range_models::reading>>{},
    R"({"first":{"value":7},"second":{"value":42}})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    std::map<std::string, int> values;
    for (const auto &entry : result.value->entries)
      values.emplace(entry.first, entry.second.value);

    EXPECT_EQ((std::map<std::string, int>{{"first", 7}, {"second", 42}}),
      values);

    EXPECT_TRUE(serialization_test::matches_json(
      R"({"first":{"value":7},"second":{"value":42}})",
      omni::ryml::as_json(*result.value)));
  }
}

TEST(range_mapping, multiset_retains_repeated_elements) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::multiset<int>>{}, "[7, 4, 7]");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_EQ((std::multiset<int>{4, 7, 7}), *result.value);
    EXPECT_TRUE(serialization_test::matches_json(
      "[4, 7, 7]", omni::ryml::as_json(*result.value)));
  }
}

TEST(range_mapping, unordered_set_uses_unique_insertion) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::unordered_set<int>>{}, "[7, 4]");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_EQ((std::unordered_set<int>{4, 7}), *result.value);
}

TEST(range_mapping, preserves_original_map_key_paths) {
  const auto result = serialization_test::deserialize(
    omni::type_t<std::map<std::string, int>>{}, R"({"a/b~c":"bad"})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  for (const auto &issue : result.diagnostics.issues) {
    EXPECT_EQ(issue_code::invalid_scalar, issue.reason);
    EXPECT_EQ("/a~1b~0c", omni::ryml::detail::render_path(issue.path));
  }
}

TEST(range_mapping, optional_object_values_are_retained) {
  const std::map<std::string, omni::compat::optional<int>> source{
    {"present", 7},
    {"empty", omni::compat::nullopt},
  };

  const std::string document = R"({"present":7,"empty":null})";
  EXPECT_TRUE(serialization_test::matches_json(
    document, omni::ryml::as_json(source)));

  const auto result = serialization_test::deserialize(
    omni::type_t<std::map<std::string, omni::compat::optional<int>>>{},
    document);

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_EQ(source, *result.value);
}

TEST(range_mapping, custom_map_enforces_its_unique_key_contract) {
  const auto result = serialization_test::deserialize(
    omni::type_t<range_models::wrapped_map<std::string, int>>{},
    R"({"port":7,"port":8})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  for (const auto &issue : result.diagnostics.issues) {
    EXPECT_EQ(issue_code::duplicate_field, issue.reason);
    EXPECT_EQ("/port", omni::ryml::detail::render_path(issue.path));
    EXPECT_FALSE(issue.warning);
  }
}

TEST(range_mapping, map_duplicate_warnings_observe_the_warning_limit) {
  const auto load = serialization_test::deserialize.transform_config(
    [](serialization_test::config options) {
      options.error_policy.duplicates =
        duplicate_policy::allow | duplicate_policy::warn;

      options.error_policy.warning_limit = 0;
      return options;
    });

  const auto result = load(omni::type_t<std::map<std::string, int>>{},
    R"({"port":7,"port":8,"other":9})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(2U, result.diagnostics.issues.size());
  if (2U == result.diagnostics.issues.size()) {
    EXPECT_EQ(issue_code::duplicate_field,
      result.diagnostics.issues.front().reason);

    EXPECT_TRUE(result.diagnostics.issues.front().warning);
    EXPECT_EQ(issue_code::warning_limit_exceeded,
      result.diagnostics.issues.back().reason);

    EXPECT_FALSE(result.diagnostics.issues.back().warning);
  }

  for (const auto &issue : result.diagnostics.issues)
    EXPECT_EQ("/port", omni::ryml::detail::render_path(issue.path));
}

TEST(range_mapping, reference_trees_borrow_map_key_characters) {
  const std::map<std::string, int> source{{"port", 7}};
  const auto tree = omni::ryml::core::as_ref_tree(source);
  EXPECT_EQ(source.begin()->first.data(),
    tree.crootref().first_child().key().str);
}

TEST(range_mapping, owning_trees_retain_map_keys_after_source_destruction) {
  ::ryml::Tree tree;
  {
    const std::map<std::string, int> source{{"port", 7}};
    tree = omni::ryml::core::as_owning_tree(source);
    EXPECT_NE(source.begin()->first.data(),
      tree.crootref().first_child().key().str);
  }

  EXPECT_EQ(c4::csubstr{"port"}, tree.crootref().first_child().key());
}

TEST(range_mapping, serializes_const_optional_values) {
  const omni::compat::optional<const std::string> source{std::string{"value"}};
  EXPECT_EQ(R"("value")", omni::ryml::as_json(source));
}

TEST(range_mapping, serializes_const_mapped_values) {
  const std::map<std::string, const std::string> source{{"name", "value"}};
  EXPECT_TRUE(serialization_test::matches_json(
    R"({"name":"value"})", omni::ryml::as_json(source)));
}

TEST(range_mapping, custom_unique_range_needs_no_emplace_or_erase_adapter) {
  const auto load = serialization_test::deserialize.transform_config(
    [](serialization_test::config options) {
      options.error_policy.duplicates = duplicate_policy::last;
      return options;
    });

  const auto result = load(omni::type_t<range_models::wrapped_values<
    range_models::keyed_value, std::set<range_models::keyed_value>>>{},
    R"([{"id":7,"label":"first"},{"id":7,"label":"last"}])");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_EQ(1U, result.value->values.size());
    if (!result.value->values.empty())
      EXPECT_EQ("last", result.value->values.begin()->label);
  }
}

struct alternate_key_traits: std::char_traits<char> {};

TEST(range_mapping, key_string_adapters_preserve_their_character_traits) {
  using key_type = std::basic_string<char, alternate_key_traits>;
  const auto result = serialization_test::deserialize(
    omni::type_t<std::map<key_type, int>>{},
    R"({"port":7})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_EQ(1U, result.value->size());
    if (!result.value->empty()) {
      EXPECT_EQ(key_type{"port"}, result.value->begin()->first);

      EXPECT_EQ(7, result.value->begin()->second);
    }

    EXPECT_TRUE(serialization_test::matches_json(
      R"({"port":7})", omni::ryml::as_json(*result.value)));
  }
}

} // namespace

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
