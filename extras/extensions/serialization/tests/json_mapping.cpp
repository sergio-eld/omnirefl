// Draft engine-independent mapping conformance tables.
// Each engine must produce the same result for a given struct, settings and JSON.
// Preserve repeated field names in the source text; do not normalize the input.
// Executable case declarations will follow once the test DSL is defined.

#include <omnirefl/serialization/ryml.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <utility>

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

namespace duplicate_fields {
/*
S = struct { int value; }
duplicates: duplicate_policy
D(duplicates, JSON) = result for S
Edup = duplicate-field error at /value
Etype = invalid-integer error at /value
Wdup = duplicate-field warning at /value

D(reject, {"value": 7})                         = ({7}, [], 0)
D(reject, {"value": 7, "value": 42})            = (none, [Edup], 0)
D(allow,  {"value": 7, "value": 42})            = ({7}, [], 0)
D(allow,  {"value": 7, "value": "bad"})         = ({7}, [], 0)
D(allow,  {"value": "bad", "value": 42})        = (none, [Etype], 0)

D(last, {"value": 7, "value": 42})              = ({42}, [], 0)
D(last, {"value": "bad", "value": 42})          = ({42}, [], 0)
D(last, {"value": 7, "value": "bad"})           = (none, [Etype], 0)

D(warn,         {"value": 7, "value": 42})      = (none, [Edup], 0)
D(allow | warn, {"value": 7, "value": 42})      = ({7}, [Wdup], 1)
D(last | warn,  {"value": 7, "value": 42})      = ({42}, [Wdup], 1)

D(allow | last, JSON) = D(last, JSON)
D(reject | warn, JSON) = D(reject, JSON)

D(reject, {"value": 1, "value": 2, "value": 3})
  = (none, [Edup], 0)
D(allow | warn, {"value": 1, "value": 2, "value": 3})
  = ({1}, [Wdup, Wdup], 2)
D(last | warn, {"value": 1, "value": 2, "value": 3})
  = ({3}, [Wdup, Wdup], 2)
D(allow | warn, {"value": "bad", "value": 42})
  = (none, [Wdup, Etype], 1)
D(last | warn, {"value": 7, "value": "bad"})
  = (none, [Wdup, Etype], 1)

D(allow, {"value": 7, "value": {}})   = ({7}, [], 0)
D(allow, {"value": 7, "value": []})   = ({7}, [], 0)
D(allow, {"value": 7, "value": null}) = ({7}, [], 0)
D(last, {"value": {}, "value": 42})   = ({42}, [], 0)
D(last, {"value": [], "value": 42})   = ({42}, [], 0)
D(last, {"value": null, "value": 42}) = ({42}, [], 0)

With error_limit = 1:
  D(reject, {"value": 1, "value": 2, "value": 3})
    = (none, [Edup, Edup], 0)
*/
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
