// Engine-independent mapping conformance cases.
// Each engine must produce the same result for a given struct, settings and
// JSON. Preserve repeated field names in the source text; do not normalize the
// input. Case groups are heterogeneous tuples, bound to an engine before
// registration.
// TODO: Collect the case groups in one struct, conceptually initialized with
// .duplicates = ..., .unknown_fields = ..., etc. Annotate each field with its
// group's description; reflection can use field names and comments to register
// and describe suites, automatically including new groups. Reuse the same data
// across engines and benchmarks, retaining heterogeneous model types per group.

#include <omnirefl/serialization/ryml.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <ostream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

/*
D/U/L/E/R/C(settings inputs, JSON) >> expect(struct value or none, ordered
issues) none = no returned value; an empty issue list means no diagnostics.
error_limit = 0; warning_limit = nullopt (unlimited); recovery = nullopt.
Each engine starts with explicit defaults; case transforms select the settings
under test. The expectation binds the struct type, independently of the engine.
All JSON is well-formed; these cases specify mapping, not parser syntax errors.
*/

namespace dsl {

template <typename TransformSettings>
struct input {
  /// Callable `Settings(Settings)`: changes one or more settings for this case
  /// in a copy of the engine's settings.
  TransformSettings transform_settings;

  std::string json;
};

struct expected_issue {
  omni::json::issue_code reason;
  omni::json::value_kind expected;
  std::string path;
  bool warning;
};

expected_issue error(omni::json::issue_code reason,
  std::string path,
  omni::json::value_kind expected = omni::json::value_kind::unknown) {
  return {reason, expected, std::move(path), false};
}

expected_issue warning(omni::json::issue_code reason, std::string path) {
  return {reason, omni::json::value_kind::unknown, std::move(path), true};
}

template <typename Model, typename TransformSettings>
struct mapping_case {
  input<TransformSettings> from;
  omni::ryml::with_diagnostics<Model, std::vector<expected_issue>> expected;

  template <typename Engine>
  omni::ryml::with_diagnostics<Model> operator()(Engine engine) const {
    return engine.transform_settings(
      from.transform_settings)(omni::type_t<Model>{}, from.json);
  }
};

template <typename Model>
omni::ryml::with_diagnostics<Model, std::vector<expected_issue>>
  expect(Model value, std::vector<expected_issue> issues) {
  return {std::move(value), std::move(issues)};
}

template <typename Model>
omni::ryml::with_diagnostics<Model, std::vector<expected_issue>>
  expect(omni::compat::nullopt_t, std::vector<expected_issue> issues) {
  return {omni::compat::nullopt, std::move(issues)};
}

// Bind the model from the expectation; the engine is supplied on invocation.
template <typename TransformSettings, typename Model>
mapping_case<Model, TransformSettings> operator>>(input<TransformSettings> from,
  omni::ryml::with_diagnostics<Model, std::vector<expected_issue>> expected) {
  return {std::move(from), std::move(expected)};
}

// Test-only value erasure keeps ValuesIn homogeneous without erasing
// diagnostics.
struct erased_value {
  std::shared_ptr<const void> storage;
  bool (*equal)(const void *, const void *);
  void (*print)(const void *, std::ostream *);

  template <typename Model>
  erased_value(Model model)
      : storage{std::make_shared<Model>(std::move(model))}
      , equal{[](const void *left, const void *right) {
        return *static_cast<const Model *>(left)
          == *static_cast<const Model *>(right);
      }}
      , print{[](const void *value, std::ostream *out) {
        *out << *static_cast<const Model *>(value);
      }} {}
};

bool operator==(const erased_value &left, const erased_value &right) {
  // Each Model specialization has its own callback; check before casting
  // storage.
  return left.equal == right.equal
    && left.equal(left.storage.get(), right.storage.get());
}

void PrintTo(const erased_value &value, std::ostream *out) {
  value.print(value.storage.get(), out);
}

bool operator==(const expected_issue &left, const expected_issue &right) {
  return std::tie(left.reason, left.expected, left.path, left.warning)
    == std::tie(right.reason, right.expected, right.path, right.warning);
}

void PrintTo(const expected_issue &issue, std::ostream *out) {
  *out << "{code: " << int(issue.reason) << ", kind: " << int(issue.expected)
       << ", path: " << issue.path << ", warning: " << issue.warning << '}';
}

bool operator==(const omni::ryml::with_diagnostics<erased_value,
                  std::vector<expected_issue>> &left,
  const omni::ryml::with_diagnostics<erased_value, std::vector<expected_issue>>
    &right) {
  return left.value == right.value && left.diagnostics == right.diagnostics;
}

void PrintTo(const omni::ryml::with_diagnostics<erased_value,
               std::vector<expected_issue>> &result,
  std::ostream *out) {
  *out << "{value: ";
  if (result.value)
    PrintTo(*result.value, out);
  else
    *out << "none";
  *out << ", issues: " << ::testing::PrintToString(result.diagnostics) << '}';
}

struct erase_value {
  template <typename Model>
  erased_value operator()(Model model) const {
    return {std::move(model)};
  }
};

struct scenario {
  std::string json;
  std::function<omni::ryml::with_diagnostics<erased_value>()> run;
  omni::ryml::with_diagnostics<erased_value, std::vector<expected_issue>>
    expected;
};

template <typename Engine, typename Case>
scenario bind_engine(Engine engine, Case c) {
  return {c.from.json,
    [engine, c] { return c(engine).map_value(erase_value{}); },
    std::move(c.expected).map_value(erase_value{})};
}

// C++11 compat for a generic capturing lambda, binding each case before
// erasure.
template <typename Engine>
struct bind_cases {
  Engine engine;

  template <typename... Cases>
  std::vector<scenario> operator()(const Cases &...cases) const {
    return {bind_engine(engine, cases)...};
  }
};

constexpr auto none = omni::compat::nullopt;

} // namespace dsl

namespace models {

struct record {
  int value;
};

struct text_record {
  std::string value;
};

bool operator==(const record &left, const record &right) {
  return left.value == right.value;
}

bool operator==(const text_record &left, const text_record &right) {
  return left.value == right.value;
}

std::ostream &operator<<(std::ostream &out, const record &value) {
  return out << "{value: " << value.value << '}';
}

std::ostream &operator<<(std::ostream &out, const text_record &value) {
  return out << "{value: \"" << value.value << "\"}";
}

struct pair_record {
  int first;
  int second;
};

struct default_record {
  int value = 17;
};

int runtime_default() {
  return 23;
}
struct runtime_record {
  int value = runtime_default();
};

struct nondefault {
  nondefault() = delete;
  explicit nondefault(int input): value{input} {}
  int value;
};
nondefault runtime_nondefault() {
  return nondefault{5};
}
struct nondefault_holder {
  nondefault value = runtime_nondefault();
};

struct nested_records {
  record nested;
  std::vector<record> items;
};
struct nested_defaults {
  default_record nested;
  std::vector<default_record> items;
};
struct optional_record {
  omni::compat::optional<record> nested;
};
struct sequence_record {
  std::vector<record> items;
};

// C++11 records with member initializers cannot be aggregate-initialized.
template <typename Record>
Record with_value(int value) {
  Record record{};
  record.value = value;
  return record;
}

template <typename Record>
std::ostream &operator<<(std::ostream &out, const std::vector<Record> &items) {
  out << '[';
  for (std::size_t index = 0; index < items.size(); ++index)
    out << (index ? ", " : "") << items[index];
  return out << ']';
}

bool operator==(const pair_record &left, const pair_record &right) {
  return std::tie(left.first, left.second)
    == std::tie(right.first, right.second);
}

std::ostream &operator<<(std::ostream &out, const pair_record &value) {
  return out << "{first: " << value.first << ", second: " << value.second
             << '}';
}

bool operator==(const default_record &left, const default_record &right) {
  return left.value == right.value;
}

std::ostream &operator<<(std::ostream &out, const default_record &value) {
  return out << "{value: " << value.value << '}';
}

bool operator==(const runtime_record &left, const runtime_record &right) {
  return left.value == right.value;
}

std::ostream &operator<<(std::ostream &out, const runtime_record &value) {
  return out << "{value: " << value.value << '}';
}

bool operator==(const nondefault_holder &left, const nondefault_holder &right) {
  return left.value.value == right.value.value;
}

std::ostream &operator<<(std::ostream &out, const nondefault_holder &value) {
  return out << "{value: {value: " << value.value.value << "}}";
}

bool operator==(const nested_records &left, const nested_records &right) {
  return left.nested == right.nested && left.items == right.items;
}

std::ostream &operator<<(std::ostream &out, const nested_records &value) {
  return out << "{nested: " << value.nested << ", items: " << value.items
             << '}';
}

bool operator==(const nested_defaults &left, const nested_defaults &right) {
  return left.nested == right.nested && left.items == right.items;
}

std::ostream &operator<<(std::ostream &out, const nested_defaults &value) {
  return out << "{nested: " << value.nested << ", items: " << value.items
             << '}';
}

bool operator==(const optional_record &left, const optional_record &right) {
  return left.nested == right.nested;
}

std::ostream &operator<<(std::ostream &out, const optional_record &value) {
  out << "{nested: ";
  if (value.nested)
    out << *value.nested;
  else
    out << "nullopt";
  return out << '}';
}

bool operator==(const sequence_record &left, const sequence_record &right) {
  return left.items == right.items;
}

std::ostream &operator<<(std::ostream &out, const sequence_record &value) {
  return out << "{items: " << value.items << '}';
}

} // namespace models

namespace duplicate_fields {

using dsl::expect;
using dsl::none;

/**
 * Overrides the duplicate policy and, when supplied, the error limit.
 *
 * C++11 equivalent of a generic capturing lambda: these fields are its inputs,
 * and the settings type is deduced from the engine.
 */
struct override_settings {
  omni::json::duplicate_policy duplicate_policy;

  // Preserve the engine's limit unless the case explicitly overrides it.
  omni::compat::optional<std::uint16_t> error_limit;

  template <typename Settings>
  OMNI_CPP14_CONSTEXPR Settings operator()(Settings settings) const {
    settings.error_policy.duplicates = duplicate_policy;
    if (error_limit)
      settings.error_policy.error_limit = *error_limit;

    return settings;
  }
};

dsl::input<override_settings> D(omni::json::duplicate_policy duplicate_policy,
  std::string json,
  omni::compat::optional<std::uint16_t> error_limit = omni::compat::nullopt) {
  return {{duplicate_policy, error_limit}, std::move(json)};
}

const dsl::expected_issue Edup{
  omni::json::issue_code::duplicate_field,
  omni::json::value_kind::unknown,
  "/value",
  false,
};

const dsl::expected_issue Etype{
  omni::json::issue_code::invalid_scalar,
  omni::json::value_kind::integer,
  "/value",
  false,
};

const dsl::expected_issue Wdup{
  omni::json::issue_code::duplicate_field,
  omni::json::value_kind::unknown,
  "/value",
  true,
};

using omni::json::duplicate_policy;

// Axes: [model][duplicate policy][valid/invalid mapping input].
// All JSON is well-formed; valid inputs produce a value and precede failures.
// D(policy, JSON) >> expect(value or none, ordered issues).
const auto duplicates = std::make_tuple(
  // [models::record][reject]
  D(duplicate_policy::reject, R"({"value": 7})")
    >> expect(models::record{7}, {}),
  D(duplicate_policy::reject, R"({"value": 7, "value": 42})")
    >> expect<models::record>(none, {Edup}),
  D(duplicate_policy::reject, R"({"value": 1, "value": 2, "value": 3})")
    >> expect<models::record>(none, {Edup}),
  D(duplicate_policy::reject,
    R"({"value": 1, "value": 2, "value": 3})",
    /*error_limit=*/1)
    >> expect<models::record>(none, {Edup, Edup}),

  // [models::record][allow]
  D(duplicate_policy::allow, R"({"value": 7, "value": 42})")
    >> expect(models::record{7}, {}),
  D(duplicate_policy::allow, R"({"value": 7, "value": "bad"})")
    >> expect(models::record{7}, {}),
  D(duplicate_policy::allow, R"({"value": 7, "value": {}})")
    >> expect(models::record{7}, {}),
  D(duplicate_policy::allow, R"({"value": 7, "value": []})")
    >> expect(models::record{7}, {}),
  D(duplicate_policy::allow, R"({"value": 7, "value": null})")
    >> expect(models::record{7}, {}),
  D(duplicate_policy::allow, R"({"value": "bad", "value": 42})")
    >> expect<models::record>(none, {Etype}),

  // [models::record][last]
  D(duplicate_policy::last, R"({"value": 7, "value": 42})")
    >> expect(models::record{42}, {}),
  D(duplicate_policy::last, R"({"value": "bad", "value": 42})")
    >> expect(models::record{42}, {}),
  D(duplicate_policy::last, R"({"value": {}, "value": 42})")
    >> expect(models::record{42}, {}),
  D(duplicate_policy::last, R"({"value": [], "value": 42})")
    >> expect(models::record{42}, {}),
  D(duplicate_policy::last, R"({"value": null, "value": 42})")
    >> expect(models::record{42}, {}),
  D(duplicate_policy::last, R"({"value": 7, "value": "bad"})")
    >> expect<models::record>(none, {Etype}),

  // [models::record][warn]
  D(duplicate_policy::warn, R"({"value": 7, "value": 42})")
    >> expect<models::record>(none, {Edup}),

  // [models::record][allow | warn]
  D(duplicate_policy::allow | duplicate_policy::warn,
    R"({"value": 7, "value": 42})")
    >> expect(models::record{7}, {Wdup}),
  D(duplicate_policy::allow | duplicate_policy::warn,
    R"({"value": 1, "value": 2, "value": 3})")
    >> expect(models::record{1}, {Wdup, Wdup}),
  D(duplicate_policy::allow | duplicate_policy::warn,
    R"({"value": "bad", "value": 42})")
    >> expect<models::record>(none, {Wdup, Etype}),

  // [models::record][last | warn]
  D(duplicate_policy::last | duplicate_policy::warn,
    R"({"value": 7, "value": 42})")
    >> expect(models::record{42}, {Wdup}),
  D(duplicate_policy::last | duplicate_policy::warn,
    R"({"value": 1, "value": 2, "value": 3})")
    >> expect(models::record{3}, {Wdup, Wdup}),
  D(duplicate_policy::last | duplicate_policy::warn,
    R"({"value": 7, "value": "bad"})")
    >> expect<models::record>(none, {Wdup, Etype}),

  // [models::record][allow | last]
  D(duplicate_policy::allow | duplicate_policy::last,
    R"({"value": 7, "value": 42})")
    >> expect(models::record{42}, {}),

  // [models::text_record][reject]
  D(duplicate_policy::reject, R"({"value": "first", "value": "last"})")
    >> expect<models::text_record>(none, {Edup}),

  // [models::text_record][allow]
  D(duplicate_policy::allow, R"({"value": "first", "value": "last"})")
    >> expect(models::text_record{"first"}, {}),

  // [models::text_record][last]
  D(duplicate_policy::last, R"({"value": "first", "value": "last"})")
    >> expect(models::text_record{"last"}, {}));

TEST(json_mapping_dsl, binds_the_model_and_transforms_engine_defaults) {
  const auto c = D(duplicate_policy::last, R"({"value": 7, "value": 42})")
    >> expect(models::record{42}, {});

  const auto result = c(omni::ryml::deserialize);
  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_EQ(c.expected.value->value, result.value->value);
  }

  EXPECT_TRUE(result.diagnostics.issues.empty());
  EXPECT_EQ(duplicate_policy::last,
    result.diagnostics.settings.error_policy.duplicates);
  EXPECT_EQ(duplicate_policy::reject,
    omni::ryml::deserialize.settings.error_policy.duplicates);
}

TEST(json_mapping_dsl, preserves_settings_outside_the_case_transformation) {
  const auto engine = omni::ryml::deserialize.transform_settings(
    [](omni::ryml::settings settings) {
      settings.error_policy.warning_limit = 0;
      settings.error_policy.error_limit = 3;
      return settings;
    });

  const auto c = D(duplicate_policy::allow | duplicate_policy::warn,
                   R"({"value": 7, "value": 42})")
    >> expect<models::record>(none,
      {
        Wdup,
        {
          omni::json::issue_code::warning_limit_exceeded,
          omni::json::value_kind::unknown,
          "/value",
          false,
        },
      });

  const auto result = c(engine);
  EXPECT_FALSE(result.value);
  EXPECT_TRUE(result.diagnostics.stopped());
  EXPECT_EQ(c.expected.diagnostics.size(), result.diagnostics.issues.size());
  if (2U == result.diagnostics.issues.size()) {
    EXPECT_EQ(c.expected.diagnostics[0].reason,
      result.diagnostics.issues[0].reason);
    EXPECT_TRUE(result.diagnostics.issues[0].warning);
    EXPECT_EQ(c.expected.diagnostics[1].reason,
      result.diagnostics.issues[1].reason);
    EXPECT_FALSE(result.diagnostics.issues[1].warning);
  }
  EXPECT_EQ(3, result.diagnostics.settings.error_policy.error_limit);
  EXPECT_EQ(duplicate_policy::reject, engine.settings.error_policy.duplicates);
}

} // namespace duplicate_fields

namespace unknown_fields {
using dsl::expect;
using dsl::none;
using omni::json::issue_code;
using omni::json::unknown_policy;

// C++11 compat for a generic capturing settings transformation.
struct override_settings {
  unknown_policy unknown_fields;

  template <typename Settings>
  Settings operator()(Settings settings) const {
    settings.error_policy.unknown_fields = unknown_fields;
    return settings;
  }
};
dsl::input<override_settings> U(unknown_policy policy, std::string json) {
  return {{policy}, std::move(json)};
}
const auto Eunknown = dsl::error(issue_code::unknown_field, "/other");
const auto Wunknown = dsl::warning(issue_code::unknown_field, "/other");

// Axes: [model][unknown-field policy][valid/invalid mapping input].
const auto cases = std::make_tuple(
  // [record][reject]
  U(unknown_policy::reject, R"({"value": 7})") >> expect(models::record{7}, {}),
  U(unknown_policy::reject, R"({"value": 7, "other": 42})")
    >> expect<models::record>(none, {Eunknown}),
  U(unknown_policy::reject, R"({"other": 1, "value": 7, "another": 2})")
    >> expect<models::record>(none, {Eunknown}),

  // [record][warn alone]: reject | warn is the same policy value.
  U(unknown_policy::warn, R"({"value": 7, "other": 42})")
    >> expect<models::record>(none, {Eunknown}),

  // [record][allow]
  U(unknown_policy::allow, R"({"value": 7, "other": 42})")
    >> expect(models::record{7}, {}),
  U(unknown_policy::allow, R"({"other": {}, "value": 7})")
    >> expect(models::record{7}, {}),

  // [record][allow | warn]; repeated unknown names still use this policy.
  U(unknown_policy::allow | unknown_policy::warn,
    R"({"value": 7, "other": 42})")
    >> expect(models::record{7}, {Wunknown}),
  U(unknown_policy::allow | unknown_policy::warn,
    R"({"other": 1, "value": 7, "another": []})")
    >> expect(models::record{7},
      {Wunknown, dsl::warning(issue_code::unknown_field, "/another")}),
  U(unknown_policy::allow | unknown_policy::warn,
    R"({"value": 7, "other": 1, "other": 2})")
    >> expect(models::record{7}, {Wunknown, Wunknown}),

  // [nested_records][allow | warn / reject]
  U(unknown_policy::allow | unknown_policy::warn,
    R"({"nested": {"value": 7, "other": 1},
        "items": [{"value": 8, "other": 2}]})")
    >> expect(models::nested_records{{7}, {{8}}},
      {dsl::warning(issue_code::unknown_field, "/nested/other"),
        dsl::warning(issue_code::unknown_field, "/items/0/other")}),
  U(unknown_policy::reject,
    R"({"nested": {"value": 7, "other": 1},
        "items": [{"value": 8, "other": 2}]})")
    >> expect<models::nested_records>(none,
      {dsl::error(issue_code::unknown_field, "/nested/other")}));
} // namespace unknown_fields

namespace warning_limits {
using dsl::expect;
using dsl::none;
using omni::json::issue_code;
using omni::json::unknown_policy;

// C++11 compat: this group allows unknown fields with warnings and overrides
// the warning limit, including an explicit nullopt for unlimited warnings.
struct override_settings {
  omni::compat::optional<std::uint16_t> warning_limit;

  template <typename Settings>
  Settings operator()(Settings settings) const {
    settings.error_policy.unknown_fields =
      unknown_policy::allow | unknown_policy::warn;
    settings.error_policy.warning_limit = warning_limit;
    return settings;
  }
};
dsl::input<override_settings> L(omni::compat::optional<std::uint16_t> limit,
  std::string json) {
  return {{limit}, std::move(json)};
}
const auto Wunknown = dsl::warning(issue_code::unknown_field, "/other");
const auto Wanother = dsl::warning(issue_code::unknown_field, "/another");
const auto Elimit = dsl::error(issue_code::warning_limit_exceeded, "/other");

// Axes: [record][warning limit][within/exceeding limit].
const auto cases = std::make_tuple(
  // [record][unlimited]
  L(none, R"({"value": 7, "other": 42})")
    >> expect(models::record{7}, {Wunknown}),
  // [record][0]
  L(0, R"({"value": 7})") >> expect(models::record{7}, {}),
  L(0, R"({"value": 7, "other": 42})")
    >> expect<models::record>(none, {Wunknown, Elimit}),
  // [record][1]
  L(1, R"({"value": 7, "other": 42})") >> expect(models::record{7}, {Wunknown}),
  L(1, R"({"value": "bad", "other": 1, "another": 2, "later": 3})")
    >> expect<models::record>(none,
      {Wunknown,
        Wanother,
        dsl::error(issue_code::warning_limit_exceeded, "/another")}),
  // [record][2]
  L(2, R"({"value": 7, "other": 1, "another": 2})")
    >> expect(models::record{7}, {Wunknown, Wanother}));
} // namespace warning_limits

namespace error_limits {
using dsl::expect;
using dsl::none;
using omni::json::issue_code;
using omni::json::unknown_policy;
using omni::json::value_kind;

// C++11 compat: error limit and unknown-field policy are this group's inputs.
struct override_settings {
  std::uint16_t error_limit;
  unknown_policy unknown_fields;

  template <typename Settings>
  Settings operator()(Settings settings) const {
    settings.error_policy.error_limit = error_limit;
    settings.error_policy.unknown_fields = unknown_fields;
    return settings;
  }
};
dsl::input<override_settings> E(std::uint16_t limit,
  std::string json,
  unknown_policy unknown = unknown_policy::reject) {
  return {{limit, unknown}, std::move(json)};
}
const auto Efirst =
  dsl::error(issue_code::invalid_scalar, "/first", value_kind::integer);
const auto Esecond =
  dsl::error(issue_code::invalid_scalar, "/second", value_kind::integer);
const auto Eunknown = dsl::error(issue_code::unknown_field, "/other");
const auto Wunknown = dsl::warning(issue_code::unknown_field, "/other");

// Axes: [pair_record][error limit][valid/invalid mapping input].
const auto cases = std::make_tuple(
  // [pair_record][0]
  E(0, R"({"first": "bad", "second": "bad"})")
    >> expect<models::pair_record>(none, {Efirst}),
  // [pair_record][1]
  E(1, R"({"first": "bad", "second": "bad"})")
    >> expect<models::pair_record>(none, {Efirst, Esecond}),
  // [pair_record][2]
  E(2, R"({"first": 7, "second": 8})") >> expect(models::pair_record{7, 8}, {}),
  E(2, R"({"first": "bad", "second": "bad"})")
    >> expect<models::pair_record>(none, {Efirst, Esecond}),

  // Warnings do not consume the error budget.
  E(0,
    R"({"other": 1, "first": "bad", "second": "bad"})",
    unknown_policy::allow | unknown_policy::warn)
    >> expect<models::pair_record>(none, {Wunknown, Efirst}),
  E(1,
    R"({"other": 1, "first": "bad", "second": "bad"})",
    unknown_policy::allow | unknown_policy::warn)
    >> expect<models::pair_record>(none, {Wunknown, Efirst, Esecond}),

  // Structural errors precede missing-field checks.
  E(0, R"({"other": 1})") >> expect<models::pair_record>(none, {Eunknown}),
  E(1, R"({"other": 1})") >> expect<models::pair_record>(none,
    {Eunknown, dsl::error(issue_code::missing_field, "/first")}),
  E(2, R"({"other": 1})") >> expect<models::pair_record>(none,
    {Eunknown,
      dsl::error(issue_code::missing_field, "/first"),
      dsl::error(issue_code::missing_field, "/second")}));
} // namespace error_limits

namespace missing_field_recovery {
using dsl::expect;
using dsl::none;
using omni::json::issue_code;
using omni::json::recovery_policy;
using omni::json::value_kind;

// C++11 compat: select missing-field recovery and the warning limit.
struct override_settings {
  omni::compat::optional<recovery_policy> recovery;
  omni::compat::optional<std::uint16_t> warning_limit;

  template <typename Settings>
  Settings operator()(Settings settings) const {
    settings.recovery = recovery;
    settings.error_policy.warning_limit = warning_limit;
    return settings;
  }
};
dsl::input<override_settings> R(
  omni::compat::optional<recovery_policy> recovery,
  std::string json,
  omni::compat::optional<std::uint16_t> warning_limit = none) {
  return {{recovery, warning_limit}, std::move(json)};
}
const auto both =
  recovery_policy::field_defaults | recovery_policy::default_initializable;
const auto Emissing = dsl::error(issue_code::missing_field, "/value");
const auto Etype =
  dsl::error(issue_code::invalid_scalar, "/value", value_kind::integer);

// Axes: [model][recovery methods][missing/present/invalid field].
const auto cases = std::make_tuple(
  // [default_record][none]
  R(none, R"({"value": 9})")
    >> expect(models::with_value<models::default_record>(9), {}),
  R(none, "{}") >> expect<models::default_record>(none, {Emissing}),

  // [default_record][field_defaults]
  R(recovery_policy::field_defaults, "{}")
    >> expect(models::default_record{}, {}),
  R(recovery_policy::field_defaults, R"({"value": 9})")
    >> expect(models::with_value<models::default_record>(9), {}),
  R(recovery_policy::field_defaults, "{}", /*warning_limit=*/0)
    >> expect(models::default_record{}, {}),
  R(recovery_policy::field_defaults, R"({"value": null})")
    >> expect<models::default_record>(none, {Etype}),

  // [default_record][default_initializable]
  R(recovery_policy::default_initializable, "{}")
    >> expect(models::with_value<models::default_record>(0), {}),
  R(recovery_policy::default_initializable, R"({"value": 9})")
    >> expect(models::with_value<models::default_record>(9), {}),
  R(recovery_policy::default_initializable, R"({"value": "bad"})")
    >> expect<models::default_record>(none, {Etype}),
  R(recovery_policy::default_initializable, R"({"value": null})")
    >> expect<models::default_record>(none, {Etype}),

  // [default_record][both]; exposed field defaults take precedence.
  R(both, "{}") >> expect(models::default_record{}, {}),
  R(both, R"({"value": 9})")
    >> expect(models::with_value<models::default_record>(9), {}),
  R(both, R"({"value": "bad"})")
    >> expect<models::default_record>(none, {Etype}),

  // [record][field_defaults / default_initializable / both]
  R(recovery_policy::field_defaults, "{}")
    >> expect<models::record>(none, {Emissing}),
  R(recovery_policy::default_initializable, "{}")
    >> expect(models::record{0}, {}),
  R(both, "{}") >> expect(models::record{0}, {}),

  // [runtime_record][field_defaults / default_initializable / both]
  R(recovery_policy::field_defaults, "{}")
    >> expect<models::runtime_record>(none, {Emissing}),
  R(recovery_policy::default_initializable, "{}")
    >> expect(models::with_value<models::runtime_record>(0), {}),
  R(both, "{}") >> expect(models::with_value<models::runtime_record>(0), {}),

  // [nondefault_holder][default_initializable / both]
  R(recovery_policy::default_initializable, "{}")
    >> expect<models::nondefault_holder>(none, {Emissing}),
  R(both, "{}") >> expect<models::nondefault_holder>(none, {Emissing}),

  // [nested_defaults][both / none]
  R(both, R"({"nested": {}, "items": [{}]})")
    >> expect(models::nested_defaults{models::default_record{},
                {models::default_record{}}},
      {}),
  R(none, R"({"nested": {}, "items": [{}]})")
    >> expect<models::nested_defaults>(none,
      {dsl::error(issue_code::missing_field, "/nested/value")}));
} // namespace missing_field_recovery

namespace policy_interactions {
using dsl::expect;
using dsl::none;
using omni::json::duplicate_policy;
using omni::json::issue_code;
using omni::json::recovery_policy;
using omni::json::unknown_policy;
using omni::json::value_kind;

// C++11 compat: combine the two warning-producing policies with
// limits/recovery.
struct override_settings {
  omni::compat::optional<std::uint16_t> warning_limit;
  omni::compat::optional<recovery_policy> recovery;

  template <typename Settings>
  Settings operator()(Settings settings) const {
    settings.error_policy.duplicates =
      duplicate_policy::allow | duplicate_policy::warn;
    settings.error_policy.unknown_fields =
      unknown_policy::allow | unknown_policy::warn;
    settings.error_policy.warning_limit = warning_limit;
    settings.recovery = recovery;
    return settings;
  }
};
dsl::input<override_settings> C(omni::compat::optional<std::uint16_t> limit,
  std::string json,
  omni::compat::optional<recovery_policy> recovery = none) {
  return {{limit, recovery}, std::move(json)};
}
const auto Wdup = dsl::warning(issue_code::duplicate_field, "/value");
const auto Wunknown = dsl::warning(issue_code::unknown_field, "/other");

// Axes: [model][warning limit/recovery][valid/invalid mapping input].
const auto cases = std::make_tuple(
  C(none, R"({"value": 7, "value": 42, "other": 5})")
    >> expect(models::record{7}, {Wdup, Wunknown}),
  C(1, R"({"value": 7, "value": 42, "other": 5})") >> expect<models::record>(
    none,
    {Wdup, Wunknown, dsl::error(issue_code::warning_limit_exceeded, "/other")}),
  C(0, "{}", recovery_policy::field_defaults)
    >> expect(models::default_record{}, {}),
  C(0, R"({"other": 5})", recovery_policy::field_defaults)
    >> expect<models::default_record>(none,
      {Wunknown, dsl::error(issue_code::warning_limit_exceeded, "/other")}),

  C(none, R"({"nested": {"value": 7, "other": 5}})")
    >> expect(models::optional_record{models::record{7}},
      {dsl::warning(issue_code::unknown_field, "/nested/other")}),
  C(none, "{}") >> expect(models::optional_record{none}, {}),
  C(none, R"({"nested": {"value": "bad"}})")
    >> expect<models::optional_record>(none,
      {dsl::error(issue_code::invalid_scalar,
        "/nested/value",
        value_kind::integer)}),

  C(none, R"({"items": [{"value": 7}, {"value": "bad"}]})")
    >> expect<models::sequence_record>(none,
      {dsl::error(issue_code::invalid_scalar,
        "/items/1/value",
        value_kind::integer)}),
  C(none,
    R"({"items": [{"value": "bad"}]})",
    recovery_policy::default_initializable)
    >> expect<models::sequence_record>(none,
      {dsl::error(issue_code::invalid_scalar,
        "/items/0/value",
        value_kind::integer)}));
} // namespace policy_interactions

// Model used by the executable interface checks below.
struct record {
  int value = 17;
};

struct optional_record {
  omni::compat::optional<record> nested;
};

TEST(json_mapping_optionals, warnings_do_not_discard_a_valid_optional_record) {
  const auto result =
    omni::ryml::deserialize.transform_settings([](omni::ryml::settings s) {
      s.error_policy.unknown_fields =
        omni::json::unknown_policy::allow | omni::json::unknown_policy::warn;
      return s;
    })(omni::type_t<optional_record>{},
      R"({"nested": {"value": 7, "other": 42}})");
  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_TRUE(result.value->nested);
    if (result.value->nested) {
      EXPECT_EQ(7, result.value->nested->value);
    }
  }
  EXPECT_EQ("/nested/other: warning: unknown field",
    omni::ryml::render_diagnostics(result.diagnostics));
}

// Probe the real method's substitution constraints without invoking mapping.
struct settings_transform_expression {
  template <typename F>
  auto operator()(F &&f) const
    -> decltype(omni::ryml::deserialize.transform_settings(std::forward<F>(f)));
};

struct wrong_settings_result {
  int operator()(omni::ryml::settings) const;
};

static_assert(
  !omni::compat::is_invocable<settings_transform_expression, int>::value,
  "settings transformation rejects non-callables");
static_assert(!omni::compat::is_invocable<settings_transform_expression,
                wrong_settings_result>::value,
  "settings transformation requires settings");

// C++11 callable equivalent of a capturing lambda; also checks constant
// evaluation when the optional implementation supports it in C++14 and newer.
struct set_error_limit {
  std::uint16_t limit;

  OMNI_CPP14_CONSTEXPR omni::ryml::settings operator()(
    omni::ryml::settings s) const {
    s.error_policy.error_limit = limit;
    return s;
  }
};

#if 201402L <= OMNI_CPLUSPLUS
constexpr auto configured =
  omni::ryml::deserialize.transform_settings(set_error_limit{3});
static_assert(3 == configured.settings.error_policy.error_limit,
  "settings transformation supports constant evaluation");
#endif

TEST(json_settings, captures_transform_a_copy_once) {
  int calls = 0;
  const std::uint16_t limit = 3;
  const auto configured = omni::ryml::deserialize.transform_settings(
    [&calls, limit](omni::ryml::settings s) {
      ++calls;
      s.error_policy.error_limit = limit;
      return s;
    });
  EXPECT_EQ(1, calls);
  EXPECT_EQ(3, configured.settings.error_policy.error_limit);
  EXPECT_EQ(0, omni::ryml::deserialize.settings.error_policy.error_limit);
}

int runtime_default() {
  return 23;
}

struct runtime_initialized_record {
  int value = runtime_default();
};

TEST(json_recovery, does_not_assume_every_initializer_is_exposed) {
  const auto result =
    omni::ryml::deserialize.transform_settings([](omni::ryml::settings s) {
      s.recovery = omni::json::recovery_policy::field_defaults;
      return s;
    })(omni::type_t<runtime_initialized_record>{}, "{}");
  EXPECT_FALSE(result.value);
  EXPECT_EQ("/value: missing field",
    omni::ryml::render_diagnostics(result.diagnostics));
}

namespace dsl {

class mapping_conformance: public ::testing::TestWithParam<scenario> {};

TEST_P(mapping_conformance, conforms_to_expected_mapping) {
  SCOPED_TRACE(GetParam().json);
  const auto actual = GetParam().run().transform_diagnostics(
    [](omni::ryml::diagnostics<true> diagnostics) {
      std::vector<dsl::expected_issue> issues;
      std::transform(diagnostics.issues.begin(),
        diagnostics.issues.end(),
        std::back_inserter(issues),
        [](const omni::ryml::issue &issue) {
          return dsl::expected_issue{issue.reason,
            issue.expected,
            omni::ryml::detail::render_path(issue.path),
            issue.warning};
        });
      return issues;
    });

  EXPECT_EQ(GetParam().expected, actual);
}

INSTANTIATE_TEST_SUITE_P(ryml_duplicates,
  mapping_conformance,
  ::testing::ValuesIn(omni::compat::apply(
    bind_cases<omni::ryml::deserialize_t>{
      omni::ryml::deserialize_t{omni::ryml::settings{}}},
    duplicate_fields::duplicates)));

INSTANTIATE_TEST_SUITE_P(ryml_unknown_fields,
  mapping_conformance,
  ::testing::ValuesIn(omni::compat::apply(
    bind_cases<omni::ryml::deserialize_t>{
      omni::ryml::deserialize_t{omni::ryml::settings{}}},
    unknown_fields::cases)));

INSTANTIATE_TEST_SUITE_P(ryml_warning_limits,
  mapping_conformance,
  ::testing::ValuesIn(omni::compat::apply(
    bind_cases<omni::ryml::deserialize_t>{
      omni::ryml::deserialize_t{omni::ryml::settings{}}},
    warning_limits::cases)));

INSTANTIATE_TEST_SUITE_P(ryml_error_limits,
  mapping_conformance,
  ::testing::ValuesIn(omni::compat::apply(
    bind_cases<omni::ryml::deserialize_t>{
      omni::ryml::deserialize_t{omni::ryml::settings{}}},
    error_limits::cases)));

INSTANTIATE_TEST_SUITE_P(ryml_missing_field_recovery,
  mapping_conformance,
  ::testing::ValuesIn(omni::compat::apply(
    bind_cases<omni::ryml::deserialize_t>{
      omni::ryml::deserialize_t{omni::ryml::settings{}}},
    missing_field_recovery::cases)));

INSTANTIATE_TEST_SUITE_P(ryml_policy_interactions,
  mapping_conformance,
  ::testing::ValuesIn(omni::compat::apply(
    bind_cases<omni::ryml::deserialize_t>{
      omni::ryml::deserialize_t{omni::ryml::settings{}}},
    policy_interactions::cases)));

} // namespace dsl

} // namespace
