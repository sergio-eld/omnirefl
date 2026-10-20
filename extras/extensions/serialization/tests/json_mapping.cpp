#if 0 // Draft: enable after the shared policy and engine adapter are wired.

// Intended coverage (pseudo form):
//   engine:       ryml; rapidjson later
//   struct:       C++ structs, instantiated at compile time
//   options:      combinations of 1..n policies, e.g. duplicates + error policy
//   json:         input documents, preserving repeated field names
//
//   cases = engine x struct x options x json
//   expected = specified_result(struct, options, json)
//
// Engines share the same expectations. A document's expected result depends on
// both the struct and policies; it is not a separate Cartesian axis.

/*
S = struct { int value; }
duplicates: duplicate_policy
D(duplicates, JSON) = (S or none, issues, warnings)
none = no value; [] = no issues
Edup = duplicate-field error at /value
Etype = invalid-integer error at /value
Wdup = duplicate-field warning at /value
error_limit = 0; warning_limit = none (unlimited); recovery = nullopt.
Omitted policy fields = defaults.

D(reject, {"value": 7})                         = ({7}, [], 0)
D(reject, {"value": 7, "value": 42})            = (none, [Edup], 0)
D(allow,  {"value": 7, "value": 42})            = ({7}, [], 0)
D(allow,  {"value": 7, "value": "bad"})         = ({7}, [], 0)
D(allow,  {"value": "bad", "value": 42})        = (none, [Etype], 0)

D(last, {"value": 7, "value": 42})    = ({42}, [], 0)
D(last, {"value": "bad", "value": 42}) = ({42}, [], 0)
D(last, {"value": 7, "value": "bad"}) = (none, [Etype], 0)

D(warn, {"value": 7, "value": 42}) = (none, [Edup], 0)
D(allow | warn, {"value": 7, "value": 42}) = ({7}, [Wdup], 1)

D(last | warn, {"value": 7, "value": 42}) = ({42}, [Wdup], 1)
D(allow | last, JSON) = D(last, JSON)
D(reject | warn, JSON) = D(reject, JSON)

For every engine:
  engine(S, error_policy{duplicates: duplicates}, JSON) = D(duplicates, JSON)

unknown_fields: unknown_policy
U(unknown_fields, JSON) = (S or none, issues, warnings)
Eunknown = unknown-field error at /other
Wunknown = unknown-field warning at /other

U(reject, {"value": 7, "other": 42}) = (none, [Eunknown], 0)
U(allow,  {"value": 7, "other": 42}) = ({7}, [], 0)
U(warn,   {"value": 7, "other": 42}) = (none, [Eunknown], 0)
U(allow | warn, {"value": 7, "other": 42}) = ({7}, [Wunknown], 1)
U(reject | warn, JSON) = U(reject, JSON)

For every engine:
  engine(S, error_policy{unknown_fields: unknown_fields}, JSON)
    = U(unknown_fields, JSON)

warnings(error_policy{duplicates: allow | warn, unknown_fields: allow | warn},
  {"value": 7, "value": 42, "other": 5}) = 2
*/

#include <omnirefl/serialization/ryml.hpp>

#include <gtest/gtest.h>

#include <string>

namespace {

struct record {
  int value;
};

// TODO: Connect the real mapper once it accepts omni::json::error_policy.
omni::ryml::with_diagnostics<record, std::string> deserialize_ryml(
  omni::json::error_policy policy,
  std::string input);

struct engine {
  // Backend label used in the parameterized test name.
  std::string name;

  // Invoke the backend and render its retained diagnostics.
  omni::ryml::with_diagnostics<record, std::string> (*deserialize)(
    omni::json::error_policy, std::string);
};

class json_mapping: public testing::TestWithParam<engine> {};

INSTANTIATE_TEST_SUITE_P(ryml,
  json_mapping,
  testing::Values(engine{"ryml", deserialize_ryml}),
  [](const testing::TestParamInfo<engine> &p) { return p.param.name; });

} // namespace

#endif
