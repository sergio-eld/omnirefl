// Draft engine-independent mapping conformance tables.
// Each engine must produce the same result for a given struct, settings and JSON.
// Preserve repeated field names in the source text; do not normalize the input.
// Duplicate cases are stored as data; the remaining tables use pseudo notation.
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
Common notation:
  result = (struct value or none, ordered issues, warning count)
  none = no returned value; [] = no issues
  error_limit = 0; warning_limit = nullopt (unlimited); recovery = nullopt
  omitted settings = defaults; each "With" block applies only to indented cases
  F_Model(...) uses the same settings as F, mapping into Model instead
  Ecode(/path) = error with that code and path
  Wcode(/path) = warning with that code and path

Cases apply to every engine. Structs are specialized at compile time; settings
and JSON are case data. Expected results are derived from these inputs, not an
independent Cartesian axis.
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

template <typename Model, typename TransformSettings>
struct mapping_case {
  input<TransformSettings> from;
  omni::ryml::with_diagnostics<Model, std::vector<expected_issue>> expected;

  template <typename Engine>
  omni::ryml::with_diagnostics<Model> operator()(Engine engine) const {
    return engine.transform_settings(from.transform_settings)(
      omni::type_t<Model>{}, from.json);
  }
};

template <typename Model>
omni::ryml::with_diagnostics<Model, std::vector<expected_issue>> expect(
  Model value,
  std::vector<expected_issue> issues) {
  return {std::move(value), std::move(issues)};
}

template <typename Model>
omni::ryml::with_diagnostics<Model, std::vector<expected_issue>> expect(
  omni::compat::nullopt_t,
  std::vector<expected_issue> issues) {
  return {omni::compat::nullopt, std::move(issues)};
}

// Bind the model from the expectation; the engine is supplied on invocation.
template <typename TransformSettings, typename Model>
mapping_case<Model, TransformSettings> operator>>(
  input<TransformSettings> from,
  omni::ryml::with_diagnostics<Model, std::vector<expected_issue>> expected) {
  return {std::move(from), std::move(expected)};
}

// Test-only value erasure keeps ValuesIn homogeneous without erasing diagnostics.
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
  // Each Model specialization has its own callback; check before casting storage.
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

bool operator==(
  const omni::ryml::with_diagnostics<erased_value,
    std::vector<expected_issue>> &left,
  const omni::ryml::with_diagnostics<erased_value,
    std::vector<expected_issue>> &right) {
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
  omni::ryml::with_diagnostics<erased_value, std::vector<expected_issue>> expected;
};

template <typename Engine, typename Case>
scenario bind_engine(Engine engine, Case c) {
  return {c.from.json,
    [engine, c] { return c(engine).map_value(erase_value{}); },
    std::move(c.expected).map_value(erase_value{})};
}

// C++11 compat for a generic capturing lambda, binding each case before erasure.
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

dsl::input<override_settings> D(
  omni::json::duplicate_policy duplicate_policy,
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
    R"({"value": 1, "value": 2, "value": 3})", /*error_limit=*/1)
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

class duplicate_mapping: public ::testing::TestWithParam<dsl::scenario> {};

TEST_P(duplicate_mapping, conforms_to_expected_mapping) {
  SCOPED_TRACE(GetParam().json);
  const auto actual = GetParam().run().transform_diagnostics(
    [](omni::ryml::diagnostics<true> diagnostics) {
      std::vector<dsl::expected_issue> issues;
      std::transform(diagnostics.issues.begin(), diagnostics.issues.end(),
        std::back_inserter(issues), [](const omni::ryml::issue &issue) {
          return dsl::expected_issue{issue.reason, issue.expected,
            omni::ryml::detail::render_path(issue.path), issue.warning};
        });
      return issues;
    });

  EXPECT_EQ(GetParam().expected, actual);
}

INSTANTIATE_TEST_SUITE_P(ryml,
  duplicate_mapping,
  ::testing::ValuesIn(omni::compat::apply(
    dsl::bind_cases<omni::ryml::deserialize_t>{
      omni::ryml::deserialize_t{omni::ryml::settings{}}},
    duplicates)));

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
  const auto engine =
    omni::ryml::deserialize.transform_settings([](omni::ryml::settings settings) {
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
/*
S = struct { int value; }
unknown_fields: unknown_policy
U(unknown_fields, JSON) = result for S
Eunknown = unknown-field error at /other
Wunknown = unknown-field warning at /other

U(reject, {"value": 7, "other": 42})           = (none, [Eunknown], 0)
U(allow,  {"value": 7, "other": 42})           = ({7}, [], 0)
U(warn,   {"value": 7, "other": 42})           = (none, [Eunknown], 0)
U(allow | warn, {"value": 7, "other": 42})      = ({7}, [Wunknown], 1)
U(reject | warn, JSON) = U(reject, JSON)

U(allow, {"other": {}, "value": 7}) = ({7}, [], 0)
U(allow | warn, {"other": 1, "value": 7, "another": []})
  = ({7}, [Wunknown, Wunknown_field(/another)], 2)
U(reject, {"other": 1, "value": 7, "another": 2})
  = (none, [Eunknown], 0)
U(allow | warn, {"value": 7, "other": 1, "other": 2})
  = ({7}, [Wunknown, Wunknown], 2)
  // Repeated unknown names still follow unknown_fields, not duplicates.

N = struct { S nested; vector<S> items; }
U_N(unknown_fields, JSON) = result for N
U_N(allow | warn,
    {"nested": {"value": 7, "other": 1},
     "items": [{"value": 8, "other": 2}]})
  = ({{7}, [{8}]},
     [Wunknown_field(/nested/other), Wunknown_field(/items/0/other)], 2)
U_N(reject,
    {"nested": {"value": 7, "other": 1},
     "items": [{"value": 8, "other": 2}]})
  = (none, [Eunknown_field(/nested/other)], 0)
*/
} // namespace unknown_fields

namespace warning_limits {
/*
S = struct { int value; }
unknown_fields = allow | warn
L(warning_limit, JSON) = result for S
Wunknown = unknown-field warning at /other
Elimit = warning-limit-exceeded error at /other

L(1, {"value": 7, "other": 42}) = ({7}, [Wunknown], 1)
L(0, {"value": 7, "other": 42}) = (none, [Wunknown, Elimit], 1)

L(nullopt, {"value": 7, "other": 42}) = ({7}, [Wunknown], 1)
L(0, {"value": 7}) = ({7}, [], 0)
L(2, {"value": 7, "other": 1, "another": 2})
  = ({7}, [Wunknown, Wunknown_field(/another)], 2)
L(1, {"value": "bad", "other": 1, "another": 2, "later": 3})
  = (none, [Wunknown, Wunknown_field(/another),
            Ewarning_limit_exceeded(/another)], 2)
  // Stop on the second warning: neither /later nor /value is processed.
*/
} // namespace warning_limits

namespace error_limits {
/*
S = struct { int first; int second; }
E(error_limit, JSON) = result for S
Efirst = invalid-scalar error at /first (expected integer)
Esecond = invalid-scalar error at /second (expected integer)

E(0, {"first": "bad", "second": "bad"}) = (none, [Efirst], 0)
E(1, {"first": "bad", "second": "bad"}) = (none, [Efirst, Esecond], 0)
E(2, {"first": "bad", "second": "bad"}) = (none, [Efirst, Esecond], 0)
E(2, {"first": 7, "second": 8}) = ({7, 8}, [], 0)

With unknown_fields = allow | warn:
  E(0, {"other": 1, "first": "bad", "second": "bad"})
    = (none, [Wunknown_field(/other), Efirst], 1)
  E(1, {"other": 1, "first": "bad", "second": "bad"})
    = (none, [Wunknown_field(/other), Efirst, Esecond], 1)

With unknown_fields = reject:
  E(0, {"other": 1}) = (none, [Eunknown_field(/other)], 0)
  E(1, {"other": 1})
    = (none, [Eunknown_field(/other), Emissing_field(/first)], 0)
  E(2, {"other": 1})
    = (none, [Eunknown_field(/other), Emissing_field(/first),
              Emissing_field(/second)], 0)
*/
} // namespace error_limits

namespace missing_field_recovery {
/*
S = struct { int value = 17; }
recovery: optional<recovery_policy>
R(recovery, JSON) = result for S
Emissing = missing-field error at /value
Etype = invalid-integer error at /value for "bad"
Enull = invalid-integer error at /value for null
both = field_defaults | default_initializable

R(nullopt,               {}) = (none, [Emissing], 0)
R(field_defaults,        {}) = ({17}, [], 0)
R(default_initializable, {}) = ({0}, [], 0)
R(both,                  {}) = ({17}, [], 0)
R(both, {"value": "bad"})    = (none, [Etype], 0)
R(field_defaults, {"value": null}) = (none, [Enull], 0)

With warning_limit = 0:
  R(field_defaults, {}) = ({17}, [], 0)

R(nullopt,               {"value": 9}) = ({9}, [], 0)
R(field_defaults,        {"value": 9}) = ({9}, [], 0)
R(default_initializable, {"value": 9}) = ({9}, [], 0)
R(both,                  {"value": 9}) = ({9}, [], 0)
R(default_initializable, {"value": "bad"}) = (none, [Etype], 0)
R(default_initializable, {"value": null})  = (none, [Enull], 0)

Plain = struct { int value; }
R_Plain(field_defaults,        {}) = (none, [Emissing], 0)
R_Plain(default_initializable, {}) = ({0}, [], 0)
R_Plain(both,                  {}) = ({0}, [], 0)

Runtime = struct { int value = runtime_default(); }
  // runtime_default() returns 23; reflection cannot expose this initializer.
R_Runtime(field_defaults,        {}) = (none, [Emissing], 0)
R_Runtime(default_initializable, {}) = ({0}, [], 0)
R_Runtime(both,                  {}) = ({0}, [], 0)

Nondefault = struct { Nondefault() = delete; explicit Nondefault(int); }
Holder = struct { Nondefault value = runtime_nondefault(); }
  // runtime_nondefault() returns Nondefault(5); its default is not exposed.
R_Holder(default_initializable, {}) = (none, [Emissing], 0)
R_Holder(both,                  {}) = (none, [Emissing], 0)

Nested = struct { S nested; vector<S> items; }
R_Nested(both, {"nested": {}, "items": [{}]})
  = ({{17}, [{17}]}, [], 0)
R_Nested(nullopt, {"nested": {}, "items": [{}]})
  = (none, [Emissing_field(/nested/value)], 0)
*/
} // namespace missing_field_recovery

namespace policy_interactions {
/*
S = struct { int value; }
duplicates = allow | warn; unknown_fields = allow | warn
C(warning_limit, JSON) = result for S
Wdup = duplicate-field warning at /value
Wunknown = unknown-field warning at /other
Elimit = warning-limit-exceeded error at /other

C(nullopt, {"value": 7, "value": 42, "other": 5})
  = ({7}, [Wdup, Wunknown], 2)
C(1, {"value": 7, "value": 42, "other": 5})
  = (none, [Wdup, Wunknown, Elimit], 2)

With recovery = field_defaults, S = struct { int value = 17; }:
  C(0, {}) = ({17}, [], 0)
  C(0, {"other": 5})
    = (none, [Wunknown, Elimit], 1)

Optional = struct { optional<S> nested; }
C_Optional(nullopt, {"nested": {"value": 7, "other": 5}})
  = ({{7}}, [Wunknown_field(/nested/other)], 1)
C_Optional(nullopt, {"nested": {"value": "bad"}})
  = (none, [Einvalid_scalar(/nested/value)], 0)
C_Optional(nullopt, {}) = ({nullopt}, [], 0)

Items = struct { vector<S> items; }
C_Items(nullopt, {"items": [{"value": 7}, {"value": "bad"}]})
  = (none, [Einvalid_scalar(/items/1/value)], 0)
With recovery = default_initializable:
  C_Items(nullopt, {"items": [{"value": "bad"}]})
    = (none, [Einvalid_scalar(/items/0/value)], 0)
*/
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

} // namespace
