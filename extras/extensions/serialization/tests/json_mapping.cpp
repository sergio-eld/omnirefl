// Experimental and subject to change; intended to explore applications of
// reflection in reusable test descriptions and registration.

// Engine-independent mapping conformance cases.
// Each engine must produce the same result for a given struct, config and
// JSON. Preserve repeated field names in the source text; do not normalize the
// input. Case groups are heterogeneous tuples, bound to an engine before
// registration.
// TODO: Collect the case groups in one struct, conceptually initialized with
// .duplicates = ..., .unknown_fields = ..., etc. Annotate each field with its
// group's description; reflection can use field names and comments to register
// and describe suites, automatically including new groups. Reuse the same data
// across engines and benchmarks, retaining heterogeneous model types per group.

#include "json_match.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

/*
D/U/L/E/C(config inputs, JSON) >> expect(struct value or none, ordered
issues) none = no returned value; an empty issue list means no diagnostics.
error_limit = 0; warning_limit = nullopt (unlimited).
Each engine starts with explicit defaults; case transforms select the config
under test. The expectation binds the struct type, independently of the engine.
All JSON is well-formed; these cases specify mapping, not parser syntax errors.
*/

namespace dsl {

template <typename TransformConfig>
struct input {
  /// Callable `Config(Config)`: changes one or more config for this case
  /// in a copy of the engine's config.
  TransformConfig transform_config;

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

expected_issue warn_unknown_field(std::string path) {
  return warning(omni::json::issue_code::unknown_field, std::move(path));
}

expected_issue err_unknown_field(std::string path) {
  return error(omni::json::issue_code::unknown_field, std::move(path));
}

template <typename Model, typename TransformConfig>
struct mapping_case {
  input<TransformConfig> from;
  omni::ryml::with_diagnostics<Model, std::vector<expected_issue>> expected;

  template <typename Engine>
  auto operator()(Engine engine) const
    -> decltype(engine.transform_config(
      from.transform_config)(omni::type_t<Model>{}, from.json)) {
    return engine.transform_config(
      from.transform_config)(omni::type_t<Model>{}, from.json);
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
template <typename TransformConfig, typename Model>
mapping_case<Model, TransformConfig> operator>>(input<TransformConfig> from,
  omni::ryml::with_diagnostics<Model, std::vector<expected_issue>> expected) {
  return {std::move(from), std::move(expected)};
}

constexpr auto none = omni::compat::nullopt;

} // namespace dsl

namespace models {

struct integer_value {
  int value;
};

struct text_value {
  std::string value;
};

struct pair_record {
  int first;
  int second;
};

struct nested_pair {
  pair_record nested;
};

struct nested_records {
  integer_value nested;
  std::vector<integer_value> items;
};

struct optional_record {
  omni::compat::optional<integer_value> nested;
};

struct sequence_record {
  std::vector<integer_value> items;
};

} // namespace models

namespace duplicate_fields {

using dsl::expect;
using dsl::none;

/**
 * Overrides the duplicate policy and, when supplied, the error limit.
 *
 * C++11 equivalent of a generic capturing lambda: these fields are its inputs,
 * and the config type is deduced from the engine.
 */
struct override_config {
  omni::json::duplicate_policy duplicate_policy;

  // Preserve the engine's limit unless the case explicitly overrides it.
  omni::compat::optional<std::uint16_t> error_limit;

  template <typename Config>
  OMNI_CPP14_CONSTEXPR Config operator()(Config config) const {
    config.error_policy.duplicates = duplicate_policy;
    if (error_limit)
      config.error_policy.error_limit = *error_limit;

    return config;
  }
};

dsl::input<override_config> D(omni::json::duplicate_policy duplicate_policy,
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
  // [models::integer_value][reject]
  D(duplicate_policy::reject, R"({"value": 7})")
    >> expect(models::integer_value{7}, {}),
  D(duplicate_policy::reject, R"({"value": 7, "value": 42})")
    >> expect<models::integer_value>(none, {Edup}),
  D(duplicate_policy::reject, R"({"value": 1, "value": 2, "value": 3})")
    >> expect<models::integer_value>(none, {Edup}),
  D(duplicate_policy::reject,
    R"({"value": 1, "value": 2, "value": 3})",
    /*error_limit=*/1)
    >> expect<models::integer_value>(none, {Edup, Edup}),

  // [models::integer_value][allow]
  D(duplicate_policy::allow, R"({"value": 7, "value": 42})")
    >> expect(models::integer_value{7}, {}),
  D(duplicate_policy::allow, R"({"value": 7, "value": "bad"})")
    >> expect(models::integer_value{7}, {}),
  D(duplicate_policy::allow, R"({"value": 7, "value": {}})")
    >> expect(models::integer_value{7}, {}),
  D(duplicate_policy::allow, R"({"value": 7, "value": []})")
    >> expect(models::integer_value{7}, {}),
  D(duplicate_policy::allow, R"({"value": 7, "value": null})")
    >> expect(models::integer_value{7}, {}),
  D(duplicate_policy::allow, R"({"value": "bad", "value": 42})")
    >> expect<models::integer_value>(none, {Etype}),

  // [models::integer_value][last]
  D(duplicate_policy::last, R"({"value": 7, "value": 42})")
    >> expect(models::integer_value{42}, {}),
  D(duplicate_policy::last, R"({"value": "bad", "value": 42})")
    >> expect(models::integer_value{42}, {}),
  D(duplicate_policy::last, R"({"value": {}, "value": 42})")
    >> expect(models::integer_value{42}, {}),
  D(duplicate_policy::last, R"({"value": [], "value": 42})")
    >> expect(models::integer_value{42}, {}),
  D(duplicate_policy::last, R"({"value": null, "value": 42})")
    >> expect(models::integer_value{42}, {}),
  D(duplicate_policy::last, R"({"value": 7, "value": "bad"})")
    >> expect<models::integer_value>(none, {Etype}),

  // [models::integer_value][warn]
  D(duplicate_policy::warn, R"({"value": 7, "value": 42})")
    >> expect<models::integer_value>(none, {Edup}),

  // [models::integer_value][allow | warn]
  D(duplicate_policy::allow | duplicate_policy::warn,
    R"({"value": 7, "value": 42})")
    >> expect(models::integer_value{7}, {Wdup}),
  D(duplicate_policy::allow | duplicate_policy::warn,
    R"({"value": 1, "value": 2, "value": 3})")
    >> expect(models::integer_value{1}, {Wdup, Wdup}),
  D(duplicate_policy::allow | duplicate_policy::warn,
    R"({"value": "bad", "value": 42})")
    >> expect<models::integer_value>(none, {Wdup, Etype}),

  // [models::integer_value][last | warn]
  D(duplicate_policy::last | duplicate_policy::warn,
    R"({"value": 7, "value": 42})")
    >> expect(models::integer_value{42}, {Wdup}),
  D(duplicate_policy::last | duplicate_policy::warn,
    R"({"value": 1, "value": 2, "value": 3})")
    >> expect(models::integer_value{3}, {Wdup, Wdup}),
  D(duplicate_policy::last | duplicate_policy::warn,
    R"({"value": 7, "value": "bad"})")
    >> expect<models::integer_value>(none, {Wdup, Etype}),

  // [models::integer_value][allow | last]
  D(duplicate_policy::allow | duplicate_policy::last,
    R"({"value": 7, "value": 42})")
    >> expect(models::integer_value{42}, {}),

  // [nested_pair][reject / allow / last]; select whole objects, never merge.
  D(duplicate_policy::reject,
    R"({"nested": {"first": 7, "second": 8},
        "nested": {"first": 9, "second": 10}})")
    >> expect<models::nested_pair>(none,
      {dsl::error(omni::json::issue_code::duplicate_field, "/nested")}),
  D(duplicate_policy::allow,
    R"({"nested": {"first": 7, "second": 8}, "nested": {"second": 10}})")
    >> expect(models::nested_pair{{7, 8}}, {}),
  D(duplicate_policy::last,
    R"({"nested": {"first": 7, "second": 8}, "nested": {"second": 10}})")
    >> expect<models::nested_pair>(none,
      {dsl::error(omni::json::issue_code::missing_field, "/nested/first")}),
  D(duplicate_policy::last,
    R"({"nested": {"second": 8}, "nested": {"first": 9, "second": 10}})")
    >> expect(models::nested_pair{{9, 10}}, {}),
  D(duplicate_policy::last | duplicate_policy::warn,
    R"({"nested": {"first": 7, "first": 9, "second": 8}})")
    >> expect(models::nested_pair{{9, 8}},
      {dsl::warning(omni::json::issue_code::duplicate_field, "/nested/first")}),

  // [sequence_record][reject / allow / last]; do not append occurrences.
  D(duplicate_policy::reject,
    R"({"items": [{"value": 7}], "items": [{"value": 9}, {"value": 10}]})")
    >> expect<models::sequence_record>(none,
      {dsl::error(omni::json::issue_code::duplicate_field, "/items")}),
  D(duplicate_policy::allow,
    R"({"items": [{"value": 7}], "items": [{"value": 9}, {"value": 10}]})")
    >> expect(models::sequence_record{{{7}}}, {}),
  D(duplicate_policy::last,
    R"({"items": [{"value": 7}], "items": [{"value": 9}, {"value": 10}]})")
    >> expect(models::sequence_record{{{9}, {10}}}, {}),

  // [models::text_value][reject]
  D(duplicate_policy::reject, R"({"value": "first", "value": "last"})")
    >> expect<models::text_value>(none, {Edup}),

  // [models::text_value][allow]
  D(duplicate_policy::allow, R"({"value": "first", "value": "last"})")
    >> expect(models::text_value{"first"}, {}),

  // [models::text_value][last]
  D(duplicate_policy::last, R"({"value": "first", "value": "last"})")
    >> expect(models::text_value{"last"}, {}));

} // namespace duplicate_fields

namespace unknown_fields {

using dsl::expect;
using dsl::none;
using dsl::err_unknown_field;
using dsl::warn_unknown_field;
using omni::json::issue_code;
using omni::json::unknown_policy;

// C++11 compat for a generic capturing config transformation.
struct override_config {
  unknown_policy unknown_fields;

  template <typename Config>
  Config operator()(Config config) const {
    config.error_policy.unknown_fields = unknown_fields;
    return config;
  }
};

dsl::input<override_config> U(unknown_policy policy, std::string json) {
  return {{policy}, std::move(json)};
}

// Axes: [model][unknown-field policy][valid/invalid mapping input].
const auto cases = std::make_tuple(
  // [integer_value][reject]
  U(unknown_policy::reject, R"({"value": 7})")
    >> expect(models::integer_value{7}, {}),
  U(unknown_policy::reject, R"({"value": 7, "other": 42})")
    >> expect<models::integer_value>(none, {err_unknown_field("/other")}),
  U(unknown_policy::reject, R"({"other": 1, "value": 7, "another": 2})")
    >> expect<models::integer_value>(none, {err_unknown_field("/other")}),

  // [integer_value][warn alone]: reject | warn is the same policy value.
  U(unknown_policy::warn, R"({"value": 7, "other": 42})")
    >> expect<models::integer_value>(none, {err_unknown_field("/other")}),

  // [integer_value][allow]
  U(unknown_policy::allow, R"({"value": 7, "other": 42})")
    >> expect(models::integer_value{7}, {}),
  U(unknown_policy::allow, R"({"other": {}, "value": 7})")
    >> expect(models::integer_value{7}, {}),

  // [integer_value][allow | warn]; also applies to repeated unknown names.
  U(unknown_policy::allow | unknown_policy::warn,
    R"({"value": 7, "other": 42})")
    >> expect(models::integer_value{7}, {warn_unknown_field("/other")}),
  U(unknown_policy::allow | unknown_policy::warn,
    R"({"other": 1, "value": 7, "another": []})")
    >> expect(models::integer_value{7},
      {warn_unknown_field("/other"), warn_unknown_field("/another")}),
  U(unknown_policy::allow | unknown_policy::warn,
    R"({"value": 7, "other": 1, "other": 2})")
    >> expect(models::integer_value{7},
      {warn_unknown_field("/other"), warn_unknown_field("/other")}),

  // [nested_records][allow | warn / reject]
  U(unknown_policy::allow | unknown_policy::warn,
    R"({"nested": {"value": 7, "other": 1},
        "items": [{"value": 8, "other": 2}]})")
    >> expect(models::nested_records{{7}, {{8}}},
      {warn_unknown_field("/nested/other"),
        warn_unknown_field("/items/0/other")}),
  U(unknown_policy::reject,
    R"({"nested": {"value": 7, "other": 1},
        "items": [{"value": 8, "other": 2}]})")
    >> expect<models::nested_records>(none,
      {err_unknown_field("/nested/other")}));

} // namespace unknown_fields

namespace warning_limits {

using dsl::expect;
using dsl::none;
using dsl::warn_unknown_field;
using omni::json::issue_code;
using omni::json::unknown_policy;

// C++11 compat: this group allows unknown fields with warnings and overrides
// the warning limit, including an explicit nullopt for unlimited warnings.
struct override_config {
  omni::compat::optional<std::uint16_t> warning_limit;

  template <typename Config>
  Config operator()(Config config) const {
    config.error_policy.unknown_fields =
      unknown_policy::allow | unknown_policy::warn;
    config.error_policy.warning_limit = warning_limit;
    return config;
  }
};

dsl::input<override_config> L(omni::compat::optional<std::uint16_t> limit,
  std::string json) {
  return {{limit}, std::move(json)};
}

// Axes: [integer_value][warning limit][within/exceeding limit].
const auto cases = std::make_tuple(
  // [integer_value][unlimited]
  L(none, R"({"value": 7, "other": 42})")
    >> expect(models::integer_value{7}, {warn_unknown_field("/other")}),
  // [integer_value][0]
  L(0, R"({"value": 7})") >> expect(models::integer_value{7}, {}),
  L(0, R"({"value": 7, "other": 42})")
    >> expect<models::integer_value>(none, {warn_unknown_field("/other"),
        dsl::error(issue_code::warning_limit_exceeded, "/other")}),
  // [integer_value][1]
  L(1, R"({"value": 7, "other": 42})")
    >> expect(models::integer_value{7}, {warn_unknown_field("/other")}),
  L(1, R"({"value": "bad", "other": 1, "another": 2, "later": 3})")
    >> expect<models::integer_value>(none,
      {warn_unknown_field("/other"),
        warn_unknown_field("/another"),
        dsl::error(issue_code::warning_limit_exceeded, "/another")}),
  // [integer_value][2]
  L(2, R"({"value": 7, "other": 1, "another": 2})")
    >> expect(models::integer_value{7},
      {warn_unknown_field("/other"), warn_unknown_field("/another")}),

  // [nested_records][1 / 2]; the warning budget spans nested objects.
  L(1,
    R"({"nested": {"value": 7, "other": 1},
        "items": [{"value": 8, "other": 2}]})")
    >> expect<models::nested_records>(none,
      {warn_unknown_field("/nested/other"),
        warn_unknown_field("/items/0/other"),
        dsl::error(issue_code::warning_limit_exceeded, "/items/0/other")}),
  L(2,
    R"({"nested": {"value": 7, "other": 1},
        "items": [{"value": 8, "other": 2}]})")
    >> expect(models::nested_records{{7}, {{8}}},
      {warn_unknown_field("/nested/other"),
        warn_unknown_field("/items/0/other")}));

} // namespace warning_limits

namespace error_limits {

using dsl::expect;
using dsl::none;
using dsl::err_unknown_field;
using dsl::warn_unknown_field;
using omni::json::issue_code;
using omni::json::unknown_policy;
using omni::json::value_kind;

// C++11 compat: error limit and unknown-field policy are this group's inputs.
struct override_config {
  std::uint16_t error_limit;
  unknown_policy unknown_fields;

  template <typename Config>
  Config operator()(Config config) const {
    config.error_policy.error_limit = error_limit;
    config.error_policy.unknown_fields = unknown_fields;
    return config;
  }
};

dsl::input<override_config> E(std::uint16_t limit,
  std::string json,
  unknown_policy unknown = unknown_policy::reject) {
  return {{limit, unknown}, std::move(json)};
}

const auto Efirst =
  dsl::error(issue_code::invalid_scalar, "/first", value_kind::integer);

const auto Esecond =
  dsl::error(issue_code::invalid_scalar, "/second", value_kind::integer);

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
    >> expect<models::pair_record>(none,
      {warn_unknown_field("/other"), Efirst}),
  E(1,
    R"({"other": 1, "first": "bad", "second": "bad"})",
    unknown_policy::allow | unknown_policy::warn)
    >> expect<models::pair_record>(none,
      {warn_unknown_field("/other"), Efirst, Esecond}),

  // Structural errors precede missing-field checks.
  E(0, R"({"other": 1})")
    >> expect<models::pair_record>(none, {err_unknown_field("/other")}),
  E(1, R"({"other": 1})") >> expect<models::pair_record>(none,
    {err_unknown_field("/other"),
      dsl::error(issue_code::missing_field, "/first")}),
  E(2, R"({"other": 1})") >> expect<models::pair_record>(none,
    {err_unknown_field("/other"),
      dsl::error(issue_code::missing_field, "/first"),
      dsl::error(issue_code::missing_field, "/second")}),

  // [nested_records][1 / 2]; child records share the error budget.
  E(1,
    R"({"nested": {"value": "bad"},
        "items": [{"value": "bad"}, {"value": "bad"}]})")
    >> expect<models::nested_records>(none,
      {dsl::error(issue_code::invalid_scalar,
        "/nested/value",
        value_kind::integer),
        dsl::error(issue_code::invalid_scalar,
          "/items/0/value",
          value_kind::integer)}),
  E(2,
    R"({"nested": {"value": "bad"},
        "items": [{"value": "bad"}, {"value": "bad"}]})")
    >> expect<models::nested_records>(none,
      {dsl::error(issue_code::invalid_scalar,
        "/nested/value",
        value_kind::integer),
        dsl::error(issue_code::invalid_scalar,
          "/items/0/value",
          value_kind::integer),
        dsl::error(issue_code::invalid_scalar,
          "/items/1/value",
          value_kind::integer)}),

  // [integer_value][wrong root kind]; valid JSON, but not a record.
  E(0, "null") >> expect<models::integer_value>(none,
    {dsl::error(issue_code::unexpected_kind, "", value_kind::mapping)}),
  E(0, "[]") >> expect<models::integer_value>(none,
    {dsl::error(issue_code::unexpected_kind, "", value_kind::mapping)}),
  E(0, "7") >> expect<models::integer_value>(none,
    {dsl::error(issue_code::unexpected_kind, "", value_kind::mapping)}));

} // namespace error_limits



namespace policy_interactions {

using dsl::expect;
using dsl::none;
using dsl::warn_unknown_field;
using omni::json::duplicate_policy;
using omni::json::issue_code;
using omni::json::unknown_policy;
using omni::json::value_kind;

// C++11 lambda equivalent: combine warning-producing policies with limits.
struct override_config {
  omni::compat::optional<std::uint16_t> warning_limit;

  template <typename Config>
  Config operator()(Config config) const {
    config.error_policy.duplicates =
      duplicate_policy::allow | duplicate_policy::warn;
    config.error_policy.unknown_fields =
      unknown_policy::allow | unknown_policy::warn;
    config.error_policy.warning_limit = warning_limit;
    return config;
  }
};

dsl::input<override_config> C(omni::compat::optional<std::uint16_t> limit,
  std::string json) { return {{limit}, std::move(json)}; }

const auto Wdup = dsl::warning(issue_code::duplicate_field, "/value");

// Axes: [model][warning limit][valid/invalid mapping input].
const auto cases = std::make_tuple(
  C(none, R"({"value": 7, "value": 42, "other": 5})")
    >> expect(models::integer_value{7}, {Wdup, warn_unknown_field("/other")}),
  C(1, R"({"value": 7, "value": 42, "other": 5})")
    >> expect<models::integer_value>(none,
      {Wdup,
        warn_unknown_field("/other"),
        dsl::error(issue_code::warning_limit_exceeded, "/other")}),
  C(none, R"({"nested": {"value": 7, "other": 5}})")
    >> expect(models::optional_record{models::integer_value{7}},
      {warn_unknown_field("/nested/other")}),
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
  C(none, R"({"items": [{"value": "bad"}]})")
    >> expect<models::sequence_record>(none,
      {dsl::error(issue_code::invalid_scalar,
        "/items/0/value",
        value_kind::integer)}));

} // namespace policy_interactions

namespace dsl {

bool operator==(const expected_issue &left, const expected_issue &right) {
  return std::tie(left.reason, left.expected, left.path, left.warning)
    == std::tie(right.reason, right.expected, right.path, right.warning);
}

void PrintTo(const expected_issue &issue, std::ostream *out) {
  *out << "{code: " << int(issue.reason) << ", kind: " << int(issue.expected)
       << ", path: " << issue.path << ", warning: " << issue.warning << '}';
}

} // namespace dsl

namespace registration {

using dsl::expected_issue;

// Compare records structurally; containers and optionals recurse into their
// values rather than requiring equality operators on every model.
struct equal_values {
  template <typename Value>
  typename std::enable_if<std::is_arithmetic<Value>::value, bool>::type
    operator()(const Value &left, const Value &right) const {
    return left == right;
  }

  bool operator()(const std::string &left, const std::string &right) const {
    return left == right;
  }

  template <typename Value>
  bool operator()(const std::vector<Value> &left,
    const std::vector<Value> &right) const {
    return left.size() == right.size()
      && std::equal(left.begin(), left.end(), right.begin(), *this);
  }

  template <typename Value>
  bool operator()(const omni::compat::optional<Value> &left,
    const omni::compat::optional<Value> &right) const {
    return bool(left) == bool(right) && (!left || (*this)(*left, *right));
  }

  template <typename Record>
  typename std::enable_if<std::is_class<Record>::value, bool>::type
    operator()(const Record &left, const Record &right) const;
};

// C++11 equivalents of capturing generic lambdas for the record fold.
template <typename Record>
struct compare_members {
  const Record &left;
  const Record &right;

  template <typename Member>
  bool operator()(bool equal, Member member) const {
    return equal && equal_values{}(member.value(left), member.value(right));
  }
};

template <typename Record>
struct compare_records {
  const Record &left;
  const Record &right;

  template <typename _M>
  bool operator()(omni::record_meta_t<_M> meta) const {
    return meta.accessible_data_members()
      | omni::fn::foldl(compare_members<Record>{left, right}, true);
  }
};

template <typename Record>
typename std::enable_if<std::is_class<Record>::value, bool>::type
  equal_values::operator()(const Record &left, const Record &right) const {
  return compare_records<Record>{left, right}(
    omni::reflected(omni::type_t<Record>{}));
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
        return omni::reflected_call(
          compare_records<Model>{*static_cast<const Model *>(left),
            *static_cast<const Model *>(right)},
          omni::type_t<Model>{});
      }}
      , print{[](const void *value, std::ostream *out) {
        *out << omni::ryml::as_json(*static_cast<const Model *>(value));
      }} {}
};

struct erase_value {
  template <typename Model>
  erased_value operator()(Model model) const { return {std::move(model)}; }
};

struct scenario {
  std::string json;
  std::function<omni::ryml::with_diagnostics<erased_value,
    serialization_test::collected_diagnostics>()> run;
  omni::ryml::with_diagnostics<erased_value, std::vector<expected_issue>>
    expected;
};

template <typename Engine, typename Case>
scenario bind_engine(Engine engine, Case c) {
  return {c.from.json,
    [engine, c] { return c(engine).transform_value(erase_value{}); },
    std::move(c.expected).transform_value(erase_value{})};
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

bool operator==(const erased_value &left, const erased_value &right) {
  // Each Model specialization has its own callback; check before casting
  // storage.
  return left.equal == right.equal
    && left.equal(left.storage.get(), right.storage.get());
}

void PrintTo(const erased_value &value, std::ostream *out) {
  value.print(value.storage.get(), out);
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

class mapping_conformance: public ::testing::TestWithParam<scenario> {};

TEST_P(mapping_conformance, conforms_to_expected_mapping) {
  SCOPED_TRACE(GetParam().json);
  const auto actual = GetParam().run().transform_diagnostics(
    [](serialization_test::collected_diagnostics diagnostics) {
      std::vector<dsl::expected_issue> issues;
      for (const auto &issue : diagnostics.issues) {
        issues.push_back({issue.reason,
          issue.expected,
          omni::ryml::detail::render_path(issue.path),
          issue.warning});
      }

      return issues;
    });

  EXPECT_EQ(GetParam().expected, actual);
}

INSTANTIATE_TEST_SUITE_P(ryml_duplicates,
  mapping_conformance,
  ::testing::ValuesIn(omni::compat::apply(
    bind_cases<serialization_test::deserialize_type>{
      serialization_test::deserialize_type{serialization_test::config{}}},
    duplicate_fields::duplicates)));

INSTANTIATE_TEST_SUITE_P(ryml_unknown_fields,
  mapping_conformance,
  ::testing::ValuesIn(omni::compat::apply(
    bind_cases<serialization_test::deserialize_type>{
      serialization_test::deserialize_type{serialization_test::config{}}},
    unknown_fields::cases)));

INSTANTIATE_TEST_SUITE_P(ryml_warning_limits,
  mapping_conformance,
  ::testing::ValuesIn(omni::compat::apply(
    bind_cases<serialization_test::deserialize_type>{
      serialization_test::deserialize_type{serialization_test::config{}}},
    warning_limits::cases)));

INSTANTIATE_TEST_SUITE_P(ryml_error_limits,
  mapping_conformance,
  ::testing::ValuesIn(omni::compat::apply(
    bind_cases<serialization_test::deserialize_type>{
      serialization_test::deserialize_type{serialization_test::config{}}},
    error_limits::cases)));

INSTANTIATE_TEST_SUITE_P(ryml_policy_interactions,
  mapping_conformance,
  ::testing::ValuesIn(omni::compat::apply(
    bind_cases<serialization_test::deserialize_type>{
      serialization_test::deserialize_type{serialization_test::config{}}},
    policy_interactions::cases)));

} // namespace registration

} // namespace
