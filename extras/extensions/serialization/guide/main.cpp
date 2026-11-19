/** Executable JSON/YAML mapping guide.
 * Examples use C++20 syntax for simplicity. Equivalent implementations are
 * possible through Omnirefl's C++11-compatible interfaces.
 *
 * Search for TEST(suite, topic) or TEST_P(suite, topic) for an entry point.
 * The contents follow source order; topics may have dedicated model namespaces.
 *
 * Basics
 *   basics.deserialize_record - default mapping interface and its result
 *   basics.deserialize_into - map a non-default-constructible record
 *   basics.serialize_record - choose JSON or YAML output
 *   basics.scalar_and_sequence_roots - documents need not be objects
 *   basics.enum_mapping - declared enumerator names as strings
 *   basics.local_configuration - customize config for an individual call
 *   basics.diagnostics - value availability, errors, and warnings
 *   basics.explicit_configuration - pass config and input explicitly
 *   basics.unknown_fields - reject, ignore, or warn about unknown fields
 *   basics.duplicate_fields - choose a repeated field occurrence
 *   basics.error_limit - collect errors before stopping
 *   basics.warning_limit - fail when the warning limit is exceeded
 *   basics.missing_fields - missing required data members are errors
 *   basics.invalid_fields - invalid input is not a missing field
 *   basics.nested_records - discover nested record types
 *   basics.sequences - vectors of records and nested vectors
 *   basics.optional_fields - missing, null, present, and invalid values
 *   basics.serialize_optional_fields - empty optionals become null
 *   basics.optional_sequence_elements - omit empty optional elements
 *   basics.key_value_ranges - map string-keyed objects
 *   basics.mapping_result - values, success markers, and structured issues
 *   basics.fundamental_mapping_results - decoded values and conversion errors
 *   basics.custom_scalar_mapping
 *     map a wrapper through the default scalar mappers
 *   basics.custom_scalar_mapping_cpp11 - equivalent C++11 struct registration
 *   basics.custom_container_mapping
 *     reuse range mapping for a container wrapper
 *   basics.custom_string_mapping - reuse string mapping for a string wrapper
 *   basics.custom_record_mapping
 *     reuse record mapping and member customizations
 *
 * Diagnostics
 *   diagnostics.structured_issues - issue codes, values, and paths
 *   diagnostics.rendering - render retained issues on demand
 *   diagnostics.custom_results - choose and compose the diagnostics result
 *   diagnostics.valid_values_with_warnings - success can retain warnings
 *
 * Advanced
 *   advanced.composed_integer_mapping - wrap a node-only expected conversion
 *   advanced.composed_boolean_mapping - construct an application flag
 *   advanced.composed_real_mapping - pass state to a value conversion
 *   advanced.composed_string_mapping - select a template result by type tag
 *   advanced.composed_arithmetic_mapping - numeric templates and diagnostics
 *   advanced.composed_enum_mapping - choose custom protocol names
 *   advanced.composed_range_mapping - construct a wrapper around a sequence
 *   advanced.composed_optional_mapping - construct nullable wrappers
 *   advanced.composed_record_mapping - nested lookup for vector elements
 *   advanced.mutating_scalar_mapping - exceptional pointer-only conversion
 *   advanced.mutating_record_mapping - existing records and state delegation
 *   advanced.tree_ownership - choose owning or borrowed string storage
 *   advanced.borrowed_input - tree and diagnostic lifetimes
 *
 * Examples
 *   customization_mapping.default_and_custom_adapters_round_trip
 *     select scalar, container, record, and custom mappings
 *   customization_mapping.standard_range_insertion
 *     reuse insertion adapters without another mapping adapter
 *   customization_mapping.field_aliases - legacy names and canonical output
 *   customization_mapping.enum_aliases - accept alternative enumerator text
 *   extended_container_mapping.appends_members_and_restores_them
 *     combine an inherited container with appended record members
 *   invalid_extended_container_mapping.rejects_invalid_contents
 *     reject missing or invalid values before modifying the destination
 *   customization_mapping_limits.collects_default_and_custom_conversion_errors
 *     report conversion failures and apply core error limits
 *   customization_optional_mapping.maps_nullable_values - null and conversion
 *   customization_mapping.user_optional_operations_reuse_mapping_adapters
 *     opt in a custom wrapper through its standard-like interface
 *   customization_mapping.omits_empty_optional_sequence_elements
 *     skip empty wrappers without dropping present zero values
 *   customization_mapping.optional_record_uses_discovered_value_type
 *     reach record metadata through an optional wrapper
 *   customization_mapping.emits_enumerator_names - serialize enum names
 *   customization_enum_mapping.accepts_only_declared_string_names
 *     validate enum input kind and name membership
 */

#include <gtest/gtest.h>

#include <omnirefl/functional.hpp>

// Mapping callables, explicit core functions, config, and diagnostics.
#include <omnirefl/serialization/ryml.hpp>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(__GNUC__) && !defined(__clang__)
// GCC warns about GTest's internal conditionals in unbraced assertion guards.
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdangling-else"
#endif

namespace {

// Defined at the end to keep document-comparison glue out of the examples.
testing::AssertionResult same_document(std::string expected,
  std::string actual);

namespace basic_mapping {

struct endpoint {
  std::string host;
  unsigned port;
};

struct service {
  std::string name;
  endpoint upstream;
  std::vector<std::string> tags;
};

struct measurement {
  explicit measurement(int initial): value(initial) {}
  int value;
};

enum class status { ready, busy };

} // namespace basic_mapping

/** The RapidYAML-backed interface accepts JSON and YAML input. deserialize is
 * a callable with default config already bound: pass the requested type and
 * input text to map it into a C++ value. See `basics.local_configuration` for
 * local customization and `basics.explicit_configuration` for explicit config.
 *
 * The result contains an optional value and retained diagnostics; a value is
 * available only on success. The default diagnostics are rendered text; see
 * `diagnostics.custom_results` for a custom result type.
 */
TEST(basics, deserialize_record) {
  // omni::type<T> supplies a type tag; C++11: omni::type_t<T>{}.
  const auto result = omni::ryml::deserialize(
    omni::type<basic_mapping::service>,
    R"(
{
  "name": "gateway",
  "upstream": {
    "host": "api.example.com",
    "port": 443
  },
  "tags": ["public", "https"]
}
)");

  EXPECT_TRUE(result.value.has_value());
  EXPECT_TRUE(result.diagnostics.empty());
  if (result.value) {
    EXPECT_EQ("gateway", result.value->name);
    EXPECT_EQ("api.example.com", result.value->upstream.host);
    EXPECT_EQ(443U, result.value->upstream.port);
    const std::vector<std::string> expected_tags{"public", "https"};
    EXPECT_EQ(expected_tags, result.value->tags);
  }
}

/** A pointer destination allows mapping a record without a default constructor.
 * The result holds a success marker (compat::monostate); the mapped value stays
 * in the supplied object. A failed mapping leaves its state unspecified.
 * `basics.mapping_result` explains the success marker in more detail. */
TEST(basics, deserialize_into) {
  basic_mapping::measurement into{7};
  const auto result = omni::ryml::deserialize(&into, R"({"value": 42})");

  EXPECT_TRUE(result.value.has_value());
  EXPECT_TRUE(result.diagnostics.empty());
  EXPECT_EQ(42, into.value);
}

/** JSON and YAML output represent the same C++ value. as_json and as_yaml
 * select the output format; deserialize accepts either format automatically. */
TEST(basics, serialize_record) {
  const basic_mapping::service source{
    "gateway",
    {"api.example.com", 443},
    {"public", "https"},
  };

  const std::string expected_json = R"(
{
  "name": "gateway",
  "upstream": {"host": "api.example.com", "port": 443},
  "tags": ["public", "https"]
}
)";

  const std::string expected_yaml = R"(
name: "gateway"
upstream:
  host: "api.example.com"
  port: 443
tags:
  - "public"
  - "https"
)";

  // Normalize formatting before comparing the expected and emitted documents.
  EXPECT_TRUE(same_document(expected_json, omni::ryml::as_json(source)));
  EXPECT_TRUE(same_document(expected_yaml, omni::ryml::as_yaml(source)));
}

// Supported scalars and vectors can be complete documents, without a record.
TEST(basics, scalar_and_sequence_roots) {
  EXPECT_EQ("42", omni::ryml::as_json(42));
  const auto scalar = omni::ryml::deserialize(omni::type<int>, "42");
  EXPECT_TRUE(scalar.value);
  if (scalar.value)
    EXPECT_EQ(42, *scalar.value);

  EXPECT_EQ("true", omni::ryml::as_json(true));
  const auto boolean = omni::ryml::deserialize(omni::type<bool>, "true");
  EXPECT_TRUE(boolean.value);
  if (boolean.value)
    EXPECT_TRUE(*boolean.value);

  EXPECT_EQ("0.5", omni::ryml::as_json(0.5));
  const auto real = omni::ryml::deserialize(omni::type<double>, "0.5");
  EXPECT_TRUE(real.value);
  if (real.value)
    EXPECT_DOUBLE_EQ(0.5, *real.value);

  const std::vector values{7, 42};
  EXPECT_TRUE(same_document("[7, 42]", omni::ryml::as_json(values)));
  EXPECT_TRUE(same_document("- 7\n- 42\n", omni::ryml::as_yaml(values)));
  const auto sequence = omni::ryml::deserialize(
    omni::type<std::vector<int>>, "[7, 42]");

  EXPECT_TRUE(sequence.value);
  if (sequence.value)
    EXPECT_EQ(values, *sequence.value);
}

/** Reflected enums use their declared names as strings in JSON and YAML.
 * Numeric values and strings without a declared name are rejected. */
TEST(basics, enum_mapping) {
  const auto result = omni::ryml::deserialize(
    omni::type<basic_mapping::status>, R"("busy")");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  if (result.value) {
    EXPECT_EQ(basic_mapping::status::busy, *result.value);
    EXPECT_EQ(R"("busy")", omni::ryml::as_json(*result.value));
  }
}

namespace policy_examples {

struct measurement { int value; };

struct two_readings {
  int first;
  int second;
};

} // namespace policy_examples

/** Local configuration customizes deserialization without changing defaults.
 * This example allows unknown fields and sets an error limit captured from
 * the calling scope. */
TEST(basics, local_configuration) {
  const std::uint16_t limit = 2;
  // C++11: spell the parameter type as omni::ryml::config_t<>.
  const auto load = omni::ryml::deserialize.tdm_config(
    [limit](omni::ryml::config_like auto s) {
      s.error_policy.error_limit = limit;
      s.error_policy.unknown_fields = omni::json::unknown_policy::allow;
      return s;
    });

  const auto result = load(omni::type<policy_examples::measurement>,
    R"({"value": 7, "trace": true})");

  EXPECT_TRUE(result.value);
  if (result.value)
    EXPECT_EQ(7, result.value->value);

  EXPECT_TRUE(result.diagnostics.empty());
  EXPECT_EQ(2, load.config.error_policy.error_limit);
  EXPECT_EQ(0, omni::ryml::deserialize.config.error_policy.error_limit);
  EXPECT_EQ(omni::json::unknown_policy::reject,
    omni::ryml::deserialize.config.error_policy.unknown_fields);
}

/** A result contains diagnostics even when no value is available. Errors
 * prevent returning a value; warnings can accompany a successful value.
 * deserialize renders text by default. `diagnostics.structured_issues` inspects
 * individual issues, `diagnostics.rendering` renders them explicitly, and
 * `diagnostics.custom_results` supplies another diagnostics representation. */
TEST(basics, diagnostics) {
  const auto result = omni::ryml::deserialize(
    omni::type<policy_examples::measurement>, R"({"value": "bad"})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/value: expected integer, found scalar", result.diagnostics);
}

/** The core operation takes configuration explicitly instead of using a bound
 * callable. */
TEST(basics, explicit_configuration) {
  // GCC 10 needs the nested policy type for aggregate deduction.
  const omni::ryml::config_t options{
    .error_policy = omni::json::error_policy{
      .unknown_fields = omni::json::unknown_policy::allow,
    },
  };
  const auto result = omni::ryml::core::deserialize(options,
    omni::type<policy_examples::measurement>,
    R"({"value": 7, "trace": true})");

  EXPECT_TRUE(result.value);
  if (result.value)
    EXPECT_EQ(7, result.value->value);

  EXPECT_TRUE(result.diagnostics.empty());
}

/** Unknown fields are rejected by default. allow ignores them; allow | warn
 * retains a warning. warn alone does not allow an unknown field.
 */
TEST(basics, unknown_fields) {
  using omni::json::unknown_policy;
  const auto load = omni::ryml::deserialize.tdm_config(
    [](omni::ryml::config_like auto s) {
      s.error_policy.unknown_fields =
        unknown_policy::allow | unknown_policy::warn;
      return s;
    });

  const auto result = load(omni::type<policy_examples::measurement>,
    R"({"value": 7, "trace": true})");

  EXPECT_TRUE(result.value);
  if (result.value)
    EXPECT_EQ(7, result.value->value);

  EXPECT_EQ("/trace: warning: unknown field", result.diagnostics);
}

/** Duplicate fields are rejected by default. allow selects the first
 * occurrence; last selects the last and implies allow. Add warn to report them
 * when accepted. Discarded occurrences are not mapped; an invalid selected
 * occurrence does not fall back to a different occurrence.
 */
TEST(basics, duplicate_fields) {
  using omni::json::duplicate_policy;
  const auto load = omni::ryml::deserialize.tdm_config(
    [](omni::ryml::config_like auto s) {
      s.error_policy.duplicates =
        duplicate_policy::last | duplicate_policy::warn;
      return s;
    });

  const auto result = load(omni::type<policy_examples::measurement>,
    R"({"value": 7, "value": 42})");

  EXPECT_TRUE(result.value);
  if (result.value)
    EXPECT_EQ(42, result.value->value);

  EXPECT_EQ("/value: warning: duplicate field", result.diagnostics);
}

/** error_limit counts errors that may be passed before stopping on the next.
 * Zero stops at the first error; one permits collecting two. Warnings do not
 * consume this budget, and increasing it never exposes an invalid value.
 */
TEST(basics, error_limit) {
  const auto load = omni::ryml::deserialize.tdm_config(
    [](omni::ryml::config_like auto s) {
      s.error_policy.error_limit = 1;
      return s;
    });

  const auto result = load(omni::type<policy_examples::two_readings>,
    R"({"first": "bad", "second": "bad"})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/first: expected integer, found scalar\n"
            "/second: expected integer, found scalar",
    result.diagnostics);
}

/** warning_limit is optional: unset means unlimited, zero fails on the first
 * warning. Exceeding it adds an error and prevents returning a value.
 */
TEST(basics, warning_limit) {
  const auto load = omni::ryml::deserialize.tdm_config(
    [](omni::ryml::config_like auto s) {
      s.error_policy.unknown_fields = omni::json::unknown_policy::allow
        | omni::json::unknown_policy::warn;
      s.error_policy.warning_limit = 0;
      return s;
    });

  const auto result = load(omni::type<policy_examples::measurement>,
    R"({"value": 7, "trace": true})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/trace: warning: unknown field\n/trace: warning limit exceeded",
    result.diagnostics);
}

// Missing required data members produce errors.
TEST(basics, missing_fields) {
  const auto result = omni::ryml::deserialize(
    omni::type<policy_examples::measurement>, "{}");

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/value: missing field", result.diagnostics);
}

// Invalid scalar values and explicit null are errors for an integer member.
TEST(basics, invalid_fields) {
  const auto load = omni::ryml::deserialize;

  const std::vector<std::string> documents{
    R"({"value": "bad"})",
    R"({"value": null})",
  };

  for (const auto &document : documents) {
    SCOPED_TRACE(document);
    const auto result = load(
      omni::type<policy_examples::measurement>, document);

    EXPECT_FALSE(result.value);
    EXPECT_FALSE(result.diagnostics.empty());
  }
}

// The outer record's metadata discovers its nested record dependencies.
TEST(basics, nested_records) {
  const auto result = omni::ryml::deserialize(
    omni::type<basic_mapping::service>,
    R"(
{
  "name": "gateway", "upstream": {"host": "localhost", "port": 8080},
  "tags": []
}
)");

  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_EQ("localhost", result.value->upstream.host);
    EXPECT_EQ(8080U, result.value->upstream.port);
    EXPECT_TRUE(result.value->tags.empty());
  }
}

namespace sequence_mapping {

struct batch {
  std::vector<policy_examples::measurement> readings;
  std::vector<std::vector<int>> groups;
};

} // namespace sequence_mapping

// Vectors can contain reflected records or further vectors.
TEST(basics, sequences) {
  const auto result = omni::ryml::deserialize(
    omni::type<sequence_mapping::batch>,
    R"(
{
  "readings": [{"value": 7}, {"value": 42}],
  "groups": [[1], [], [2, 3]]
}
)");

  EXPECT_TRUE(result.value);
  if (result.value) {
    std::vector<int> values;
    for (const auto &reading : result.value->readings)
      values.push_back(reading.value);

    const std::vector expected_values{7, 42};
    const std::vector<std::vector<int>> expected_groups{{1}, {}, {2, 3}};
    EXPECT_EQ(expected_values, values);
    EXPECT_EQ(expected_groups, result.value->groups);
  }
}

namespace optional_mapping {

struct measurement {
  omni::compat::optional<int> value;
};

} // namespace optional_mapping

/** An optional field accepts a missing field or explicit null as no value.
 * A supplied invalid value remains an error; optional does not hide it.
 */
TEST(basics, optional_fields) {
  const std::vector<std::string> absent{"{}", R"({"value": null})"};
  for (const auto &document : absent) {
    SCOPED_TRACE(document);
    const auto result = omni::ryml::deserialize(
      omni::type<optional_mapping::measurement>, document);

    EXPECT_TRUE(result.value);
    if (result.value)
      EXPECT_FALSE(result.value->value);
  }

  const auto present = omni::ryml::deserialize(
    omni::type<optional_mapping::measurement>, R"({"value": 7})");

  EXPECT_TRUE(present.value);
  if (present.value) {
    EXPECT_TRUE(present.value->value);
    if (present.value->value) {
      EXPECT_EQ(7, *present.value->value);
    }
  }

  const auto invalid = omni::ryml::deserialize(
    omni::type<optional_mapping::measurement>, R"({"value": "bad"})");

  EXPECT_FALSE(invalid.value);
}

// Empty optional fields emit null; populated fields emit their value.
TEST(basics, serialize_optional_fields) {
  const optional_mapping::measurement empty{omni::compat::nullopt};
  EXPECT_TRUE(same_document(R"({"value": null})", omni::ryml::as_json(empty)));
  EXPECT_TRUE(same_document("value: null", omni::ryml::as_yaml(empty)));

  const optional_mapping::measurement present{7};
  EXPECT_TRUE(same_document(R"({"value": 7})", omni::ryml::as_json(present)));
}

/** Empty optional sequence elements are omitted. Remaining elements keep their
 * order, so their output indices can differ from their original indices. */
TEST(basics, optional_sequence_elements) {
  const std::vector<omni::compat::optional<int>> values{
    7, omni::compat::nullopt, 0, omni::compat::nullopt,
  };

  EXPECT_TRUE(same_document("[7, 0]", omni::ryml::as_json(values)));
  EXPECT_TRUE(same_document("- 7\n- 0", omni::ryml::as_yaml(values)));
}

/** Key/value ranges map to objects. Unique keys follow duplicate policies;
 * non-unique ranges such as multimap retain every repeated name. */
TEST(basics, key_value_ranges) {
  const std::map<std::string, int> source{{"first", 7}, {"second", 42}};
  const std::string json = R"({"first": 7, "second": 42})";
  EXPECT_TRUE(same_document(json, omni::ryml::as_json(source)));
  EXPECT_TRUE(same_document(json, omni::ryml::as_yaml(source)));
  const auto result = omni::ryml::deserialize(
    omni::type<std::map<std::string, int>>, json);

  EXPECT_TRUE(result.value);
  if (result.value)
    EXPECT_EQ(source, *result.value);
}

/** map_by_schema returns with_diagnostics_t<T>: .value is optional<T>, and
 * .diagnostics contains structured issues rather than the default deserialize
 * binding's rendered text. An engaged .value means success, possibly with
 * warnings; an empty .value means failure. The source tree stays alive while
 * its node references and diagnostic paths are used.
 *
 * A pointer-based node reader updates an existing T, so its result uses
 * with_diagnostics_t<compat::monostate>. The engaged monostate marks success;
 * the decoded object is in the supplied destination. On failure, .value is
 * empty and .diagnostics describes the errors. The destination state is then
 * unspecified and may contain partial updates; failure promises no rollback.
 *
 * Custom readers return this result to the enclosing mapper, which collects
 * its issues and applies the error/warning limits. Reading state supplies the
 * config and accumulated counts; it is passed down explicitly,
 * rather than used as a mutable output for diagnostics.
 * See `diagnostics.structured_issues` for individual issue fields. */
TEST(basics, mapping_result) {
  const auto tree = omni::ryml::core::parse("42");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto result = omni::ryml::map_by_schema(omni::type<int>, *tree);

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_EQ(42, *result.value);
}

/** Fundamental readers return a decoded value and structured diagnostics
 * when given a type tag. */
TEST(basics, fundamental_mapping_results) {
  const auto tree = omni::ryml::core::parse(
    R"({"boolean": true, "integer": 7, "real": 2.5, "text": "hello"})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const omni::ryml::config_t options{};
  const omni::ryml::read_state_t state{
    .config = options,
    .error_count = 0,
    .warning_count = 0,
  };

  const auto node = tree->crootref();
  const auto boolean = omni::ryml::default_to_boolean(
    node["boolean"], omni::type<bool>, state);
  const auto integer = omni::ryml::default_to_integral(
    node["integer"], omni::type<int>, state);
  const auto natural = omni::ryml::default_to_integral(
    node["integer"], omni::type<unsigned>, state);
  const auto real = omni::ryml::default_to_floating_point(
    node["real"], omni::type<double>, state);
  const auto arithmetic = omni::ryml::default_to_arithmetic(
    node["integer"], omni::type<int>, state);
  const auto text = omni::ryml::default_to_string_like(
    node["text"], omni::type<std::string>, state);
  const auto view = omni::ryml::default_to_string_like(
    node["text"], omni::type<omni::compat::string_view>, state);

  EXPECT_EQ(true, boolean.value);
  EXPECT_EQ(7, integer.value);
  EXPECT_EQ(7U, natural.value);
  EXPECT_EQ(2.5, real.value);
  EXPECT_EQ(7, arithmetic.value);
  EXPECT_EQ("hello", text.value);
  EXPECT_EQ(omni::compat::string_view{"hello"}, view.value);

  const auto invalid = omni::ryml::default_to_integral(
    node["text"], omni::type<int>, state);

  EXPECT_FALSE(invalid.value);
  EXPECT_FALSE(invalid.diagnostics.issues.empty());
}

namespace basic_customization {

struct my_integer { int value; };

} // namespace basic_customization

} // namespace

// Register one reader and writer per type. The callable form is available
// since C++14; the struct form below is its C++11 equivalent.
template <>
const auto omni::ryml::cp::to_type<basic_customization::my_integer> =
  [](::ryml::ConstNodeRef node,
    omni::type_t<basic_customization::my_integer>,
    omni::ryml::read_state_like auto state)
    -> omni::ryml::with_diagnostics_t<basic_customization::my_integer> {
    return omni::ryml::default_to_integral(node,
      omni::type<decltype(basic_customization::my_integer::value)>,
      std::move(state))
      .transform([](auto value) -> basic_customization::my_integer {
        return {value};
      });
  };

template <>
const auto omni::ryml::cp::from_type<basic_customization::my_integer> =
  [](const basic_customization::my_integer &value,
    ::ryml::NodeRef node,
    omni::ryml::write_state_like auto state) -> void {
    omni::ryml::from_integral(value.value, node, std::move(state));
  };

namespace {

/** A wrapper need not map to an object. to_type and from_type select its
 * representation globally; only one registration form is used per direction.
 * transform wraps the decoded integer and keeps its diagnostics.
 * These callables use default_to_integral for integer checks and diagnostics
 * and from_integral for writing. write_state_like and read_state_like identify
 * each direction's state without spelling its implementation parameters. The
 * generic signatures delay instantiation until called within reflected scope.
 * `basics.custom_scalar_mapping_cpp11` shows the trait-based equivalent. */
TEST(basics, custom_scalar_mapping) {
  const auto result = omni::ryml::deserialize(
    omni::type<basic_customization::my_integer>, "42");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  if (result.value) {
    EXPECT_EQ(42, result.value->value);
    EXPECT_EQ("42", omni::ryml::as_json(*result.value));
  }
}

namespace basic_customization {

struct my_cpp11_integer { int value; };

} // namespace basic_customization

} // namespace

// One C++11 equivalent: the same scalar mapping needs named operator() bodies.
template <>
struct omni::ryml::cp::to_type_t<basic_customization::my_cpp11_integer> {
  OMNI_CONSTRAINED_TEMPLATE((typename State),
    (omni::ryml::is_read_state<State>::value))
  auto operator()(::ryml::ConstNodeRef node,
    omni::type_t<basic_customization::my_cpp11_integer>,
    State state) const
    -> omni::ryml::with_diagnostics_t<basic_customization::my_cpp11_integer> {
    return omni::ryml::default_to_integral(node,
      omni::type_t<decltype(basic_customization::my_cpp11_integer::value)>{},
      std::move(state))
      .transform(
        [](decltype(basic_customization::my_cpp11_integer::value) value)
          -> basic_customization::my_cpp11_integer { return {value}; });
  }
};

template <>
struct omni::ryml::cp::from_type_t<basic_customization::my_cpp11_integer> {
  OMNI_CONSTRAINED_TEMPLATE((typename State),
    (omni::ryml::is_write_state<State>::value))
  void operator()(const basic_customization::my_cpp11_integer &value,
    ::ryml::NodeRef node,
    State state) const {
    omni::ryml::from_integral(value.value, node, std::move(state));
  }
};

namespace {

/** Before C++14, specialize to_type_t and from_type_t instead of variable
 * templates. my_cpp11_integer uses the equivalent named callables above. */
TEST(basics, custom_scalar_mapping_cpp11) {
  const auto result = omni::ryml::deserialize(
    omni::type_t<basic_customization::my_cpp11_integer>{}, "42");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  if (result.value) {
    EXPECT_EQ(42, result.value->value);
    EXPECT_EQ("42", omni::ryml::as_json(*result.value));
  }
}

namespace basic_customization {

struct my_container { std::vector<int> values; };

} // namespace basic_customization

} // namespace

template <>
const auto omni::ryml::cp::to_type<basic_customization::my_container> =
  [](::ryml::ConstNodeRef node, basic_customization::my_container *into,
    omni::ryml::read_state_like auto state)
    -> omni::ryml::with_diagnostics_t<omni::compat::monostate> {
    return omni::ryml::default_to_range(omni::type<decltype(into->values)>)(
      node, &into->values, std::move(state));
  };

template <>
const auto omni::ryml::cp::from_type<basic_customization::my_container> =
  [](const basic_customization::my_container &value,
    ::ryml::NodeRef node,
    omni::ryml::write_state_like auto state) -> void {
    omni::ryml::from_range(value.values, node, std::move(state));
  };

namespace {

/** A wrapper can use default_to_range and from_range for its container.
 * The built-in mappings handle insertion, element mapping, and diagnostics;
 * the specialization only exposes the wrapped container. */
TEST(basics, custom_container_mapping) {
  const auto result = omni::ryml::deserialize(
    omni::type<basic_customization::my_container>, "[7, 42]");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  if (result.value) {
    const std::vector expected{7, 42};
    EXPECT_EQ(expected, result.value->values);
    EXPECT_TRUE(same_document("[7, 42]", omni::ryml::as_json(*result.value)));
  }
}

namespace basic_customization {

struct my_string { std::string value; };

} // namespace basic_customization

} // namespace

template <>
const auto omni::ryml::cp::to_type<basic_customization::my_string> =
  [](::ryml::ConstNodeRef node, basic_customization::my_string *into,
    omni::ryml::read_state_like auto state)
    -> omni::ryml::with_diagnostics_t<omni::compat::monostate> {
    return omni::ryml::default_to_string_like(
      node, &into->value, std::move(state));
  };

template <>
const auto omni::ryml::cp::from_type<basic_customization::my_string> =
  [](const basic_customization::my_string &value,
    ::ryml::NodeRef node,
    omni::ryml::write_state_like auto state) -> void {
    omni::ryml::from_string_like(value.value, node, std::move(state));
  };

namespace {

/** default_to_string_like and from_string_like provide the built-in string
 * mapping.
 * A wrapper can delegate to them using its stored string; custom string APIs
 * can instead adapt the shared string operations. */
TEST(basics, custom_string_mapping) {
  const auto result = omni::ryml::deserialize(
    omni::type<basic_customization::my_string>, R"("gateway")");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  if (result.value) {
    EXPECT_EQ("gateway", result.value->value);
    EXPECT_EQ(R"("gateway")", omni::ryml::as_json(*result.value));
  }
}

namespace basic_customization {

struct my_record {
  my_integer reading;
  std::string name;
};

} // namespace basic_customization

} // namespace

// Reader signature: (ConstNodeRef, T *, read_state_like)
//   -> with_diagnostics_t<monostate>.
// Exposition: spell out state forwarding; direct registration can reuse
// default_to_record itself. The generic destination delays metadata queries.
template <>
const auto omni::ryml::cp::to_type<basic_customization::my_record> =
  []<typename T>(::ryml::ConstNodeRef node, T *into,
    omni::ryml::read_state_like auto state)
    -> omni::ryml::with_diagnostics_t<omni::compat::monostate> {
    // Delegate with the current configuration and diagnostic counters.
    return omni::ryml::reflected_scope::default_to_record(omni::type<T>)(
      node, into, std::move(state));
  };

template <>
const auto omni::ryml::cp::from_type<basic_customization::my_record> =
  omni::ryml::reflected_scope::from_record;

namespace {

/** default_to_record and from_record map accessible non-static members.
 * They remain available for custom registrations. The reader receives the
 * node, destination pointer, and mapping state. It returns success or failure
 * with diagnostics. A custom mapping passes the current state when
 * delegating, including to child nodes. Paths are derived from the source
 * tree when issues are reported. See the extended-container example for
 * accumulated diagnostics.
 * Member mappings still use their own customization points: my_record::reading
 * uses the scalar representation of my_integer. */
TEST(basics, custom_record_mapping) {
  const std::string document = R"({"reading": 7, "name": "sensor"})";
  const auto result = omni::ryml::deserialize(
    omni::type<basic_customization::my_record>, document);

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  if (result.value) {
    EXPECT_EQ(7, result.value->reading.value);
    EXPECT_EQ("sensor", result.value->name);
    EXPECT_TRUE(same_document(document, omni::ryml::as_json(*result.value)));
  }
}

/** Inspect issue codes and field/index paths with the caller-owned tree.
 * node_id identifies the original node, including duplicate-key occurrences.
 * Keep the tree and its character storage alive while using these diagnostics.
 */
TEST(diagnostics, structured_issues) {
  const auto tree = omni::ryml::core::parse(R"({"value": "bad"})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto result = omni::ryml::map_by_schema(
    omni::type<policy_examples::measurement>, *tree);

  EXPECT_EQ(1U, result.diagnostics.issues.size());
  for (const auto &issue : result.diagnostics.issues) {
    EXPECT_EQ(omni::json::issue_code::unexpected_kind, issue.reason);
    EXPECT_EQ(omni::json::value_kind::integer, issue.expected);
    EXPECT_EQ(omni::json::value_kind::scalar, issue.actual);
    EXPECT_EQ("bad", tree->val(issue.node_id));
    EXPECT_EQ(1U, issue.path.size());
    for (const auto &segment : issue.path) {
      EXPECT_EQ("value", segment.field);
      EXPECT_FALSE(segment.index);
    }
  }
}

/** Render mapping diagnostics using their source tree.
 * Messages use JSON Pointer paths, with <root> for a root-level issue.
 * The default deserialize binding already returns these rendered messages.
 */
TEST(diagnostics, rendering) {
  const auto tree = omni::ryml::core::parse(R"({"value": "bad"})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto result = omni::ryml::map_by_schema(
    omni::type<policy_examples::measurement>, *tree);

  const std::string message =
    omni::ryml::render_diagnostics(*tree, result.diagnostics);

  EXPECT_EQ("/value: expected integer, found scalar", message);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
}

/** Diagnostics can be application data rather than text alone. The configured
 * renderer receives the source tree and collected issues before the tree is
 * released. This example decorates the current renderer with an issue count.
 * tdm_config and tdm_transform_diagnostics immediately produce configured
 * copies; rendering happens when the resulting deserialize callable is used. */
TEST(diagnostics, custom_results) {
  struct summary {
    std::size_t issue_count;
    std::string message;
  };

  const auto load = omni::ryml::deserialize.tdm_config(
    [](omni::ryml::config_like auto options) {
      return options.tdm_transform_diagnostics(
        [](omni::ryml::render_diagnostics_t previous) {
          return [previous](const ::ryml::Tree &tree,
            omni::ryml::diagnostics report) {
            return summary{
              .issue_count = report.issues.size(),
              .message = previous(tree, report),
            };
          };
        });
    });

  const auto success = load(
    omni::type<policy_examples::measurement>, R"({"value": 7})");

  EXPECT_TRUE(success.value);
  if (success.value)
    EXPECT_EQ(7, success.value->value);

  EXPECT_EQ(0U, success.diagnostics.issue_count);
  EXPECT_TRUE(success.diagnostics.message.empty());

  const auto failure = load(
    omni::type<policy_examples::measurement>, R"({"value": "bad"})");

  EXPECT_FALSE(failure.value);
  EXPECT_EQ(1U, failure.diagnostics.issue_count);
  EXPECT_EQ("/value: expected integer, found scalar", failure.diagnostics.message);
}

// Accepted input conditions can produce warnings alongside a valid value.
TEST(diagnostics, valid_values_with_warnings) {
  const auto load = omni::ryml::deserialize.tdm_config(
    [](omni::ryml::config_like auto s) {
      s.error_policy.duplicates = omni::json::duplicate_policy::allow
        | omni::json::duplicate_policy::warn;
      return s;
    });

  const auto result = load(omni::type<policy_examples::measurement>,
    R"({"value": 7, "value": 42})");

  EXPECT_TRUE(result.value);
  if (result.value)
    EXPECT_EQ(7, result.value->value);

  EXPECT_EQ("/value: warning: duplicate field", result.diagnostics);
}

} // namespace

/** Shape compositors validate the node before invoking a custom conversion.
 * A callback can return expected<T, issue>, expected<T, input_range<issue>>,
 * or with_diagnostics_t<T>. Errors with an explicit source node retain their
 * document path. Otherwise, paths are relative to the callback input node.
 * The resulting callable registers through cp::to_type, map_by_schema's
 * customization point; deserialize uses the same lookup.
 * Value-producing callbacks are preferred. The exceptional pointer-only
 * callbacks appear after the value-producing examples below. */
namespace composed_integer {

struct my_int { int value; };

// std::from_chars is C++17. Earlier standards can use the core numeric
// conversion with state, as in advanced.composed_arithmetic_mapping.
auto parse(::ryml::ConstNodeRef node)
  -> omni::compat::expected<my_int, omni::ryml::issue> {
  int value{};
  const auto parsed = std::from_chars(node.val().begin(), node.val().end(),
    value);

  if (parsed.ec != std::errc{} || parsed.ptr != node.val().end())
    return omni::compat::unexpected{omni::ryml::issue{
      .reason = parsed.ec == std::errc::result_out_of_range
        ? omni::json::issue_code::out_of_range
        : omni::json::issue_code::invalid_scalar,
      .expected = omni::json::value_kind::integer,
      .actual = omni::json::value_kind::unknown,
      .path = {},
    }};

  return my_int{value};
}

} // namespace composed_integer

template <>
const auto omni::ryml::cp::to_type<composed_integer::my_int> =
  omni::ryml::compose::to_integral(composed_integer::parse);

/** A node-only conversion constructs my_int; to_integral checks the input
 * shape and adapts expected to the mapping result. Compare the direct
 * registration in basics.custom_scalar_mapping. */
TEST(advanced, composed_integer_mapping) {
  const auto result = omni::ryml::deserialize(
    omni::type<composed_integer::my_int>, "42");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  if (result.value)
    EXPECT_EQ(42, result.value->value);

  const auto wrong_shape = omni::ryml::deserialize(
    omni::type<composed_integer::my_int>, R"("42")");

  EXPECT_FALSE(wrong_shape.value);
  EXPECT_EQ("<root>: expected integer, found scalar",
    wrong_shape.diagnostics);
}

namespace composed_boolean {

struct feature_flag { bool enabled; };

} // namespace composed_boolean

template <>
const auto omni::ryml::cp::to_type<composed_boolean::feature_flag> =
  omni::ryml::compose::to_boolean([](::ryml::ConstNodeRef node)
    -> omni::compat::expected<composed_boolean::feature_flag,
      omni::ryml::issue> {
    // to_boolean has already checked the true/false spelling.
    return composed_boolean::feature_flag{
      *omni::ryml::core::parse_boolean({node.val().str, node.val().len})};
  });

/** to_boolean accepts Boolean scalars and constructs an application flag. */
TEST(advanced, composed_boolean_mapping) {
  const auto result = omni::ryml::deserialize(
    omni::type<composed_boolean::feature_flag>, "true");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  if (result.value)
    EXPECT_TRUE(result.value->enabled);
}

namespace composed_real {

struct temperature { double celsius; };

} // namespace composed_real

template <>
const auto omni::ryml::cp::to_type<composed_real::temperature> =
  omni::ryml::compose::to_floating_point([](::ryml::ConstNodeRef node,
    omni::ryml::read_state_like auto state)
    -> omni::compat::expected<composed_real::temperature,
      omni::ryml::issue> {
    auto decoded = omni::ryml::core::map_node_to_floating_point(
      node, omni::type<double>, std::move(state));

    if (!decoded.value)
      return omni::compat::unexpected{omni::ryml::issue{
        // This scalar's error is relative to the input node, without a prefix.
        .reason = decoded.diagnostics.issues.front().reason,
        .expected = omni::json::value_kind::number,
        .actual = omni::json::value_kind::unknown,
        .path = {},
      }};

    return composed_real::temperature{*decoded.value};
  });

/** A (node, state) callback reuses numeric conversion and returns expected.
 * The state carries the caller's policies; to_floating_point supplies the
 * shape check. */
TEST(advanced, composed_real_mapping) {
  const auto result = omni::ryml::deserialize(
    omni::type<composed_real::temperature>, "21.5");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  if (result.value)
    EXPECT_DOUBLE_EQ(21.5, result.value->celsius);
}

namespace composed_string {

template <typename Text>
struct label { Text value; };

} // namespace composed_string

// C++14/17: use cp::to_type's Enable parameter with
// compat::enable_if_t<traits::is_string_like<Text>::value>.
template <typename Text>
  requires omni::traits::is_string_like<Text>::value
const auto omni::ryml::cp::to_type<composed_string::label<Text>> =
  omni::ryml::compose::to_string_like([](::ryml::ConstNodeRef node,
    omni::type_t<composed_string::label<Text>>)
    -> omni::compat::expected<composed_string::label<Text>,
      omni::ryml::issue> {
    return composed_string::label<Text>{omni::adapt::from_string(
      omni::type<Text>,
      omni::compat::string_view{node.val().str, node.val().len})};
  });

/** A template family uses a type tag to select its result type.
 * to_string_like checks the node before the callback constructs its value
 * through the shared string adapter. This example uses owning std::string storage. */
TEST(advanced, composed_string_mapping) {
  const auto result = omni::ryml::deserialize(
    omni::type<composed_string::label<std::string>>, R"("gateway")");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  if (result.value)
    EXPECT_EQ("gateway", result.value->value);
}

namespace composed_arithmetic {

template <typename T>
struct number { T value; };

} // namespace composed_arithmetic

template <typename T>
  requires std::is_arithmetic_v<T>
const auto omni::ryml::cp::to_type<composed_arithmetic::number<T>> =
  omni::ryml::compose::to_arithmetic([](::ryml::ConstNodeRef node,
    omni::type_t<composed_arithmetic::number<T>>,
    omni::ryml::read_state_like auto state)
    -> omni::ryml::with_diagnostics_t<composed_arithmetic::number<T>> {
    return omni::ryml::core::map_node_to_arithmetic(node,
      omni::type<decltype(composed_arithmetic::number<T>::value)>,
      std::move(state))
      .transform([](auto value) -> composed_arithmetic::number<T> {
        return {value};
      });
  });

/** (node, type<T>, state) supports numeric template families.
 * with_diagnostics preserves the core conversion's complete diagnostics;
 * to_arithmetic accepts Boolean, integer, and real inputs. */
TEST(advanced, composed_arithmetic_mapping) {
  const auto integer = omni::ryml::deserialize(
    omni::type<composed_arithmetic::number<int>>, "42");

  EXPECT_TRUE(integer.value);
  EXPECT_TRUE(integer.diagnostics.empty());
  if (integer.value)
    EXPECT_EQ(42, integer.value->value);

  const auto real = omni::ryml::deserialize(
    omni::type<composed_arithmetic::number<double>>, "2.5");

  EXPECT_TRUE(real.value);
  EXPECT_TRUE(real.diagnostics.empty());
  if (real.value)
    EXPECT_DOUBLE_EQ(2.5, real.value->value);
}

namespace composed_enum {

enum class mode { idle, active };

} // namespace composed_enum

template <>
const auto omni::ryml::cp::to_type<composed_enum::mode> =
  omni::ryml::compose::to_enum([](::ryml::ConstNodeRef node)
    -> omni::compat::expected<composed_enum::mode, omni::ryml::issue> {
    if (node.val() == "standby")
      return composed_enum::mode::idle;

    if (node.val() == "running")
      return composed_enum::mode::active;

    return omni::compat::unexpected{omni::ryml::issue{
      .reason = omni::json::issue_code::invalid_scalar,
      .expected = omni::json::value_kind::string,
      .actual = omni::json::value_kind::unknown,
      .path = {},
    }};
  });

/** to_enum checks for string input; the callback defines the accepted names.
 * Here the protocol's names differ from the C++ enumerator names. */
TEST(advanced, composed_enum_mapping) {
  const auto result = omni::ryml::deserialize(
    omni::type<composed_enum::mode>, R"("running")");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  if (result.value)
    EXPECT_EQ(composed_enum::mode::active, *result.value);

  const auto unknown = omni::ryml::deserialize(
    omni::type<composed_enum::mode>, R"("paused")");

  EXPECT_FALSE(unknown.value);
  EXPECT_FALSE(unknown.diagnostics.empty());
}

namespace composed_range {

struct readings { std::vector<int> values; };

} // namespace composed_range

template <>
const auto omni::ryml::cp::to_type<composed_range::readings> =
  omni::ryml::compose::to_range([](::ryml::ConstNodeRef node,
    omni::ryml::read_state_like auto state)
    -> omni::ryml::with_diagnostics_t<composed_range::readings> {
    composed_range::readings value{};
    auto mapped = omni::ryml::to_range_t{}(
      node, &value.values, std::move(state));

    return {
      .value = mapped.value
        ? omni::compat::make_optional(std::move(value)) : omni::compat::nullopt,
      .diagnostics = std::move(mapped.diagnostics),
    };
  });

/** to_range supplies the array check. The callback constructs its wrapper
 * and delegates element conversion to the built-in range mapping. */
TEST(advanced, composed_range_mapping) {
  const auto result = omni::ryml::deserialize(
    omni::type<composed_range::readings>, "[4, 8, 15]");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  if (result.value)
    EXPECT_EQ((std::vector{4, 8, 15}), result.value->values);
}

namespace composed_optional {

struct limit { omni::compat::optional<int> value; };

} // namespace composed_optional

template <>
const auto omni::ryml::cp::to_type<composed_optional::limit> =
  omni::ryml::compose::to_optional_like([](::ryml::ConstNodeRef node,
    omni::ryml::read_state_like auto state)
    -> omni::ryml::with_diagnostics_t<composed_optional::limit> {
    composed_optional::limit value{};
    auto mapped = omni::ryml::to_optional_like_t{}(
      node, &value.value, std::move(state));

    return {
      .value = mapped.value
        ? omni::compat::make_optional(std::move(value)) : omni::compat::nullopt,
      .diagnostics = std::move(mapped.diagnostics),
    };
  });

/** to_optional_like accepts null or a value. The callback supplies null's
 * meaning for its wrapper and delegates a present value's conversion. */
TEST(advanced, composed_optional_mapping) {
  const auto absent = omni::ryml::deserialize(
    omni::type<composed_optional::limit>, "null");

  EXPECT_TRUE(absent.value);
  EXPECT_TRUE(absent.diagnostics.empty());
  if (absent.value)
    EXPECT_FALSE(absent.value->value);

  const auto present = omni::ryml::deserialize(
    omni::type<composed_optional::limit>, "42");

  EXPECT_TRUE(present.value);
  EXPECT_TRUE(present.diagnostics.empty());
  if (present.value) {
    EXPECT_TRUE(present.value->value);
    if (present.value->value)
      EXPECT_EQ(42, *present.value->value);
  }
}

namespace composed_record {

struct inner { int value; };
struct outer { std::vector<inner> entries; };

} // namespace composed_record

template <>
const auto omni::ryml::cp::to_type<composed_record::inner> =
  omni::ryml::compose::to_record([](::ryml::ConstNodeRef node,
    omni::ryml::read_state_like auto state)
    -> omni::ryml::with_diagnostics_t<composed_record::inner> {
    composed_record::inner value{};
    // Bypass this registration for inner; its members keep their own lookup.
    auto mapped = omni::ryml::reflected_scope::to_record_t<>{}(
      node, &value, std::move(state));

    return {
      .value = mapped.value
        ? omni::compat::make_optional(
            composed_record::inner{std::max(0, value.value)})
        : omni::compat::nullopt,
      .diagnostics = std::move(mapped.diagnostics),
    };
  });

/** Only inner has a custom registration. Deserializing outer uses its
 * defaults, then selects that registration for each vector element. The
 * customization clamps negative readings to zero after field conversion.
 * to_record checks object input before the callback traverses inner's
 * fields. */
TEST(advanced, composed_record_mapping) {
  const auto result = omni::ryml::deserialize(
    omni::type<composed_record::outer>,
    R"({"entries": [{"value": -7}, {"value": 42}]})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  if (result.value) {
    EXPECT_EQ(2U, result.value->entries.size());
    if (result.value->entries.size() == 2) {
      EXPECT_EQ(0, result.value->entries[0].value);
      EXPECT_EQ(42, result.value->entries[1].value);
    }
  }

  const auto wrong_shape = omni::ryml::deserialize(
    omni::type<composed_record::outer>, R"({"entries": [7]})");

  EXPECT_FALSE(wrong_shape.value);
  EXPECT_EQ("/entries/0: expected object, found scalar",
    wrong_shape.diagnostics);
}

namespace mutating_scalar {

struct descriptor {
  int value;

  descriptor() = delete;
  explicit descriptor(int value): value(value) {}
};

} // namespace mutating_scalar

template <>
const auto omni::ryml::cp::to_type<mutating_scalar::descriptor> =
  omni::ryml::compose::to_integral([](::ryml::ConstNodeRef node,
    mutating_scalar::descriptor *into)
    -> omni::compat::expected<omni::compat::monostate, omni::ryml::issue> {
    auto decoded = composed_integer::parse(node);
    if (!decoded)
      return omni::compat::unexpected{std::move(decoded.error())};

    into->value = decoded->value;
    return omni::compat::monostate{};
  });

/** Exceptional pointer-only customization for an existing object with no
 * default constructor: (node, T*) -> expected<monostate, issue>.
 * No type<T> overload is synthesized. map_by_schema and deserialize prefer
 * value-producing type<T> overloads when available and assignable; T* is a
 * fallback, with no retry after a value-producing mapping fails.
 * Mapping errors leave the destination state unspecified. */
TEST(advanced, mutating_scalar_mapping) {
  mutating_scalar::descriptor into{7};
  const auto result = omni::ryml::deserialize(&into, "42");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  EXPECT_EQ(42, into.value);
}

namespace mutating_record {

struct settings {
  int port;

  settings() = delete;
  explicit settings(int port): port(port) {}
};

} // namespace mutating_record

template <>
const auto omni::ryml::cp::to_type<mutating_record::settings> =
  omni::ryml::compose::to_record([](::ryml::ConstNodeRef node,
    auto *into, omni::ryml::read_state_like auto state)
    -> omni::ryml::with_diagnostics_t<omni::compat::monostate> {
    return omni::ryml::reflected_scope::to_record_t<>{}(
      node, into, std::move(state));
  });

/** (node, T*, state) delegates field conversion with the current configuration
 * and counters. Mutation is useful here because settings has no default
 * constructor; it does not promise to append sequences or preserve nested
 * record state. Value-producing callbacks remain the preferred interface. */
TEST(advanced, mutating_record_mapping) {
  mutating_record::settings into{80};
  const auto result = omni::ryml::deserialize(&into, R"({"port": 443})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  EXPECT_EQ(443, into.port);
}

namespace {

namespace tree_ownership {

struct reading {
  std::string label;
  omni::compat::optional<int> value;
};

} // namespace tree_ownership

/** core::as_ref_tree borrows existing strings; keep their storage alive and
 * unchanged while using the tree. core::as_owning_tree copies strings into
 * its arena so the tree can outlive the source. Both trees own their nodes
 * and generated scalar text. The returned trees can be edited before emission.
 */
TEST(advanced, tree_ownership) {
  const tree_ownership::reading source{"sensor", omni::compat::nullopt};
  const auto borrowed = omni::ryml::core::as_ref_tree(source);
  EXPECT_TRUE(same_document(R"({"label": "sensor", "value": null})",
    ::ryml::emitrs_json<std::string>(borrowed)));

  auto owning = omni::ryml::core::as_owning_tree(source);
  owning.rootref().remove_child("value");
  EXPECT_TRUE(same_document(R"({"label": "sensor"})",
    ::ryml::emitrs_json<std::string>(owning)));

  EXPECT_TRUE(same_document(R"({"label": "sensor"})",
    ::ryml::emitrs_yaml<std::string>(owning)));
}

/** map_by_schema borrows its input; the caller keeps the tree and character
 * storage alive while using borrowed values or rendering diagnostics.
 * A subtree can be mapped independently; diagnostic paths retain its
 * position in the document.
 */
TEST(advanced, borrowed_input) {
  const auto tree = omni::ryml::core::parse(R"({"entry": {"value": "bad"}})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto result = omni::ryml::map_by_schema(
    omni::type<policy_examples::measurement>, tree->crootref()["entry"]);

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/entry/value: expected integer, found scalar",
    omni::ryml::render_diagnostics(*tree, result.diagnostics));
}

// Normalize either format to JSON text; report parse failures as test failures.
testing::AssertionResult same_document(std::string expected,
  std::string actual) {
  auto expected_tree = omni::ryml::core::parse(std::move(expected));
  if (!expected_tree)
    return testing::AssertionFailure() << "Invalid expected document: "
      << expected_tree.error();

  auto actual_tree = omni::ryml::core::parse(std::move(actual));
  if (!actual_tree)
    return testing::AssertionFailure() << "Invalid output: "
      << actual_tree.error();

  // Normalize container layout while preserving scalar quoting.
  for (auto *tree : {&*expected_tree, &*actual_tree}) {
    tree->rootref().set_style_conditionally(
      /*type_mask=*/::ryml::NOTYPE,
      /*rem_style_flags=*/::ryml::CONTAINER_STYLE,
      /*add_style_flags=*/::ryml::NOTYPE,
      /*recurse=*/true);
  }

  const std::string expected_json =
    ::ryml::emitrs_json<std::string>(*expected_tree);

  const std::string actual_json =
    ::ryml::emitrs_json<std::string>(*actual_tree);

  return expected_json == actual_json
    ? testing::AssertionSuccess()
    : testing::AssertionFailure() << "Expected: " << expected_json
      << "\nActual: " << actual_json;
}

} // namespace

// Tests: models, custom specializations, then test bodies.
namespace customization_mapping {

enum class status { ready, busy, null };

struct integer {
  int value;
};

// Adapt the engagement check without changing the optional type.
template <typename T>
struct optional_value {
  using value_type = T;

  bool engaged;
  T value;

  explicit operator bool() const { return this->engaged; }
  const T &operator*() const { return this->value; }
  void reset() { this->engaged = false; }
  void emplace(T value) {
    this->value = std::move(value);
    this->engaged = true;
  }
};

struct entry {
  int value;
  std::string name;
};

/** Only optional_record discovers this type; earlier calls cannot supply
 * metadata. */
struct optional_payload {
  int value;
};

struct optional_record {
  omni::compat::optional<optional_payload> item;
};

struct record {
  int count;
  std::vector<entry> entries;
  integer reading;
};

struct extended_values: std::vector<int> {
  int first;
  int second;
};

} // namespace customization_mapping

namespace omni {
namespace traits {
namespace cp {

// Adapt engagement; other operations use their member-based defaults.
template <typename T>
struct has_value<customization_mapping::optional_value<T>> {
  bool operator()(const customization_mapping::optional_value<T> &value) const {
    return static_cast<bool>(value);
  }
};

} // namespace cp
} // namespace traits

namespace ryml {

/** cp::to_type customizes map_by_schema's node-to-type mapping; deserialize
 * uses the same lookup. Custom readers specialize the callable variable
 * to_type<T> or the C++11-compatible to_type_t<T>; registering both is an error.
 *
```
cp::to_type<T> :: {
  (node, type<T>, state) -> with_diagnostics<T>,
  (node, T*, state)      -> with_diagnostics<monostate>
}
```
 *
 * Either or both overloads may be provided; to_type_t<T>{} follows the same
 * contract. map_by_schema and deserialize prefer value production when its
 * result is assignable, without retrying T* after a mapping error.
 * Shape compositors above adapt simpler callbacks to this contract. Writers
 * register cp::from_type<T> or cp::from_type_t<T>; unregistered types use defaults.
 *
 * read_state_like constrains a reader's state without naming its template
 * parameters. The state borrows the configuration and carries the current
 * path and accumulated error/warning counts. Pass it unchanged when delegating
 * the same node; use child(...) when descending to another key or index.
 * Child paths borrow their parent state, which must stay alive and unchanged
 * until that child mapping finishes.
 * write_state_like constrains the writer's state, which carries the string
 * storage policy. Forward it unchanged to nested writers.
 * Before C++20, is_read_state<State> and is_write_state<State> provide the
 * corresponding Boolean traits; C++14 also provides their _v forms.
 *
 * The pointer-based examples below return with_diagnostics_t<monostate>:
 * an engaged value signals success, and diagnostics carry their issues.
 * When a customization uses metadata, keep its queries dependent on template
 * arguments so their bodies are instantiated after metadata is available.
 * Non-dependent metadata queries are checked too early. */
namespace cp {

// A configured default compositor can be registered without a wrapper.
#if 0
// TODO: [high] Move alias configuration to the model customization API.
template <>
const auto to_type<customization_mapping::record> =
  reflected_scope::to_record.tdm_name_by_alias(
    [](const auto &) {
      return aliases_dict(std::tuple{"count", "legacy_count"});
    });
#endif

template <>
const auto from_type<customization_mapping::record> =
  reflected_scope::from_record;

#if 0
// TODO: [high] Move alias configuration to the model customization API.
template <>
const auto to_type<customization_mapping::status> =
  reflected_scope::to_enum.tdm_value_by_alias(
    [](const auto &) { return aliases_dict(std::tuple{"busy", "working"}); });
#endif

template <>
const auto from_type<customization_mapping::status> =
  reflected_scope::from_enum;

/** Ad hoc: inheriting standard containers is discouraged. This custom mapping
 * needs the derived record's metadata as well as container iteration. */
template <>
struct reflect_as_root<customization_mapping::extended_values>:
    std::true_type {};

/** Record metadata describes the added fields; container adapters handle the
 * flat representation. The fields occupy its final two positions. */
template <>
struct to_type_t<customization_mapping::extended_values> {
  template <typename T, omni::ryml::read_state_like State>
  auto operator()(::ryml::ConstNodeRef node,
    T *into,
    State context) const
    -> with_diagnostics_t<compat::monostate, diagnostics> {
    T values;
    auto result = default_to_range(omni::type<T>)(node, &values, context);
    if (!result.value)
      return result;

    const auto binding = omni::meta_for<T>::bind(*into);
    const auto members = binding.accessible_data_members();
    const auto count = std::tuple_size<decltype(members)>::value;
    if (values.size() < count) {
      auto failure = detail::make_node_accumulator(context);
      failure.append(std::move(result));
      failure.report(core::make_issue(json::issue_code::out_of_range,
        json::value_kind::sequence,
        node));

      return std::move(failure).finish();
    }

    const auto tail = std::next(values.cbegin(), values.size() - count);
    members | fn::foldl(
      [](auto from, omni::data_member_binding auto field) -> decltype(from) {
        field.set_value(*from);
        return std::next(from);
      },
      tail);
    into->assign(values.cbegin(), tail);
    return result;
  }
};

template <>
struct from_type_t<customization_mapping::extended_values> {
  template <typename T, omni::ryml::write_state_like State>
  void operator()(const T &value,
    ::ryml::NodeRef node,
    State context) const {
    from_range(value, node, std::move(context));

    const auto binding = omni::meta_for<T>::bind(value);
    binding.accessible_data_members()
      | fn::each(
        [&node, context](omni::data_member_binding auto field) -> void {
          core::write_node(field.value(), node.append_child(), context);
        });
  }
};

template <>
struct to_type_t<customization_mapping::integer> {
  template <omni::ryml::read_state_like State>
  auto operator()(::ryml::ConstNodeRef node,
    customization_mapping::integer *into,
    State context) const
    -> with_diagnostics_t<compat::monostate, diagnostics> {
    int converted{};
    auto result = core::read_node(node, &converted, context);
    if (!result.value)
      return result;

    if (converted < 4 || 8 < converted) {
      auto failure = detail::make_node_accumulator(context);
      failure.append(std::move(result));
      failure.report(core::make_issue(json::issue_code::out_of_range,
        json::value_kind::integer,
        node));

      return std::move(failure).finish();
    }

    into->value = converted;
    return result;
  }
};

template <>
struct from_type_t<customization_mapping::integer> {
  template <omni::ryml::write_state_like State>
  void operator()(const customization_mapping::integer &value,
    ::ryml::NodeRef node,
    State context) const {
    core::write_node(value.value, node, std::move(context));
  }
};

} // namespace cp
} // namespace ryml
} // namespace omni

namespace {

/** A custom scalar representation also applies to members and container
 * elements. The integer wrapper writes its contained number and checks its
 * application range when reading; records and vectors keep their defaults. */
TEST(customization_mapping, default_and_custom_adapters_round_trip) {
  const customization_mapping::record source{
    2,
    {{7, "first"}, {8, "second"}},
    {6},
  };

  const auto document = omni::ryml::as_json(source);
  EXPECT_TRUE(same_document(R"(
{
  "count": 2,
  "entries": [{"value": 7, "name": "first"},
              {"value": 8, "name": "second"}],
  "reading": 6
}
)",

    document));

  const auto result = omni::ryml::deserialize(
    omni::type<customization_mapping::record>, document);

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  if (!result.value)
    return;

  const auto &into = *result.value;
  EXPECT_EQ(source.count, into.count);
  EXPECT_EQ(source.reading.value, into.reading.value);
  EXPECT_EQ(source.entries.size(), into.entries.size());
  if (source.entries.size() == into.entries.size()) {
    for (std::size_t i = 0; i < source.entries.size(); ++i) {
      SCOPED_TRACE(i);
      EXPECT_EQ(source.entries[i].value, into.entries[i].value);
      EXPECT_EQ(source.entries[i].name, into.entries[i].name);
    }
  }
}

// Container mapping stays the same when insertion uses a different operation.
TEST(customization_mapping, standard_range_insertion) {
  const auto result = omni::ryml::deserialize(
    omni::type<std::set<int>>, "[7, 4]");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  if (result.value) {
    EXPECT_EQ((std::set<int>{4, 7}), *result.value);
    EXPECT_TRUE(same_document("[4, 7]", omni::ryml::as_json(*result.value)));
  }
}

/** Field aliases accept legacy documents while serialization keeps canonical
 * member names. aliases_dict groups a canonical name with its aliases.
 * A present canonical name takes precedence; duplicate policies apply
 * among aliases when the canonical name is absent.
 * Alias sets must not overlap other members' names or aliases. The dictionary
 * borrows its names; these literals have static storage. */
#if 0
// TODO: [high] Restore this example through model alias configuration.
TEST(customization_mapping, field_aliases) {
  const auto result = omni::ryml::deserialize(
    omni::type<customization_mapping::record>,
    R"({"legacy_count": 2, "entries": [], "reading": 6})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  if (!result.value)
    return;

  EXPECT_EQ(2, result.value->count);
  EXPECT_TRUE(same_document(R"({"count": 2, "entries": [], "reading": 6})",
    omni::ryml::as_json(*result.value)));

  const auto canonical = omni::ryml::deserialize(
    omni::type<customization_mapping::record>,
    R"(
{
  "count": 7,
  "legacy_count": "ignored",
  "entries": [],
  "reading": 6
}
)");

  EXPECT_TRUE(canonical.value);
  EXPECT_TRUE(canonical.diagnostics.empty());
  if (canonical.value)
    EXPECT_EQ(7, canonical.value->count);
}

#endif

/** Enum aliases accept alternative text without changing serialized enumerator
 * names. The configured enum compositor still rejects unknown text and values
 * whose node kind is not a string. */
#if 0
// TODO: [high] Restore this example through model alias configuration.
TEST(customization_mapping, enum_aliases) {
  const auto result = omni::ryml::deserialize(
    omni::type<customization_mapping::status>, R"("working")");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  if (!result.value)
    return;

  EXPECT_EQ(customization_mapping::status::busy, *result.value);
  EXPECT_EQ(R"("busy")", omni::ryml::as_json(*result.value));
}

#endif

struct extended_container_case {
  std::string name;
  customization_mapping::extended_values source;
  std::string document;
};

class extended_container_mapping:
    public testing::TestWithParam<extended_container_case> {};

/** Explicit record customization combines the inherited container contents with
 * the additional members: [base elements..., first, second]. Deserialization
 * restores the final values into those members rather than the base container.
 */
TEST_P(extended_container_mapping, appends_members_and_restores_them) {
  const auto &c = GetParam();
  EXPECT_TRUE(same_document(c.document, omni::ryml::as_json(c.source)));

  const auto result = omni::ryml::deserialize(
    omni::type<customization_mapping::extended_values>, c.document);

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  if (!result.value)
    return;

  EXPECT_EQ(static_cast<const std::vector<int> &>(c.source),
    static_cast<const std::vector<int> &>(*result.value));

  EXPECT_EQ(c.source.first, result.value->first);
  EXPECT_EQ(c.source.second, result.value->second);
}

INSTANTIATE_TEST_SUITE_P(flat_arrays,
  extended_container_mapping,
  testing::ValuesIn(std::vector<extended_container_case>{
    {"with_elements", {{1, 2}, 4, 8}, "[1, 2, 4, 8]"},
    {"empty_base", {{}, 4, 8}, "[4, 8]"},
  }),

  [](const testing::TestParamInfo<extended_container_case> &info) {
    return info.param.name;
  });

struct invalid_extended_container_case {
  std::string name;
  std::string document;
  customization_mapping::extended_values initial;
  omni::json::issue_code reason;
  std::string path;
};

class invalid_extended_container_mapping:
    public testing::TestWithParam<invalid_extended_container_case> {};

TEST_P(invalid_extended_container_mapping, rejects_invalid_contents) {
  const auto &c = GetParam();
  auto parsed = omni::ryml::core::parse(c.document);
  EXPECT_TRUE(parsed);
  if (!parsed)
    return;

  const omni::ryml::config_t options{};
  const omni::ryml::read_state_t context{
    .config = options,
    .error_count = 0,
    .warning_count = 0,
  };

  auto into = c.initial;
  const auto mapped = omni::reflected_call(
    [node = parsed->crootref(), context](omni::record_binding auto record)
      -> omni::ryml::with_diagnostics_t<omni::compat::monostate> {
      return omni::ryml::core::read_node(node, &record.value(), context);
    },
    into);

  EXPECT_FALSE(mapped.value);
  EXPECT_EQ(static_cast<const std::vector<int> &>(c.initial),
    static_cast<const std::vector<int> &>(into));

  EXPECT_EQ(c.initial.first, into.first);
  EXPECT_EQ(c.initial.second, into.second);
  EXPECT_EQ(1U, mapped.diagnostics.issues.size());
  if (1U == mapped.diagnostics.issues.size()) {
    EXPECT_EQ(c.reason, mapped.diagnostics.issues.front().reason);
    EXPECT_EQ(c.path,
      omni::ryml::detail::render_path(mapped.diagnostics.issues.front().path));
  }
}

INSTANTIATE_TEST_SUITE_P(flat_arrays,
  invalid_extended_container_mapping,
  testing::ValuesIn(std::vector<invalid_extended_container_case>{
    {
      "missing_tail",
      "[7]",
      {{99}, 3, 6},
      omni::json::issue_code::out_of_range,
      "",
    },
    {
      "invalid_tail",
      R"([1, 2, 4, "bad"])",
      {{10, 20}, 5, 9},
      omni::json::issue_code::unexpected_kind,
      "/3",
    },
    {
      "invalid_prefix",
      R"(["bad", 4, 8])",
      {{}, 2, 7},
      omni::json::issue_code::unexpected_kind,
      "/0",
    },
  }),

  [](const testing::TestParamInfo<invalid_extended_container_case> &info) {
    return info.param.name;
  });

class customization_mapping_limits:
    public testing::TestWithParam<std::uint16_t> {};

// Enclosing mappers accumulate custom readers' issues and apply limits.
TEST_P(customization_mapping_limits,
  collects_default_and_custom_conversion_errors) {
  // GCC 10 needs the nested policy type for aggregate deduction.
  const omni::ryml::config_t options{
    .error_policy = omni::json::error_policy{.error_limit = GetParam()},
  };
  const auto tree = omni::ryml::core::parse(R"(
{
  "count": 2,
  "entries": [{"value": "bad", "name": "first"}],
  "reading": 9
}
)");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto result = omni::ryml::core::map_by_schema(options,
    omni::type<customization_mapping::record>, *tree);

  EXPECT_FALSE(result.value);
  const auto &issues = result.diagnostics.issues;
  const std::size_t expected_errors = 0 == GetParam() ? 1 : 2;
  EXPECT_EQ(expected_errors, issues.size());
  if (!issues.empty()) {
    EXPECT_EQ(omni::json::issue_code::unexpected_kind, issues[0].reason);
    EXPECT_FALSE(issues[0].warning);
    EXPECT_EQ("/entries/0/value",
      omni::ryml::detail::render_path(issues[0].path));
  }

  if (2U == issues.size()) {
    EXPECT_EQ(omni::json::issue_code::out_of_range, issues[1].reason);
    EXPECT_FALSE(issues[1].warning);
    EXPECT_EQ("/reading", omni::ryml::detail::render_path(issues[1].path));
  }
}

INSTANTIATE_TEST_SUITE_P(error_limits,
  customization_mapping_limits,
  testing::Values(std::uint16_t{0}, std::uint16_t{2}));

struct optional_mapping_case {
  std::string name;
  std::string document;
  omni::compat::optional<int> initial;
  omni::compat::expected<omni::compat::optional<int>, omni::json::issue_code>
    expected;
};

class customization_optional_mapping:
    public testing::TestWithParam<optional_mapping_case> {};

/** Optional mapping updates a present value and resets on null. Conversion
 * failure preserves the existing destination and returns diagnostic issues. */
TEST_P(customization_optional_mapping, maps_nullable_values) {
  const auto &c = GetParam();
  auto parsed = omni::ryml::core::parse(c.document);
  EXPECT_TRUE(parsed);
  if (!parsed)
    return;

  const omni::ryml::config_t options{};
  const omni::ryml::read_state_t context{
    .config = options,
    .error_count = 0,
    .warning_count = 0,
  };

  auto into = c.initial;
  const auto mapped = omni::ryml::core::read_node(
    parsed->crootref(), &into, context);

  EXPECT_EQ(bool(c.expected), bool(mapped.value));
  if (c.expected) {
    EXPECT_EQ(*c.expected, into);
    EXPECT_TRUE(mapped.diagnostics.issues.empty());

    EXPECT_TRUE(same_document(c.document, omni::ryml::as_json(into)));
  } else {
    EXPECT_EQ(c.initial, into);
    EXPECT_EQ(1U, mapped.diagnostics.issues.size());
    if (1U == mapped.diagnostics.issues.size())
      EXPECT_EQ(c.expected.error(), mapped.diagnostics.issues.front().reason);
  }
}

INSTANTIATE_TEST_SUITE_P(nullable,
  customization_optional_mapping,
  testing::ValuesIn(std::vector<optional_mapping_case>{
    {
      "present",
      "7",
      omni::compat::nullopt,
      omni::compat::optional<int>{7},
    },
    {"zero", "0", omni::compat::nullopt, omni::compat::optional<int>{0}},
    {"null", "null", 99, omni::compat::optional<int>{}},
    {
      "invalid_value",
      R"("bad")",
      99,
      omni::compat::unexpected<omni::json::issue_code>{
        omni::json::issue_code::unexpected_kind,
      },
    },
  }),

  [](const testing::TestParamInfo<optional_mapping_case> &info) {
    return info.param.name;
  });

/** Shared optional-operation adapters let a wrapper use the default optional
 * mapper without a serialization-specific registration. */
TEST(customization_mapping, user_optional_operations_reuse_mapping_adapters) {
  const auto result = omni::ryml::deserialize(
    omni::type<customization_mapping::optional_value<int>>, "7");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  if (result.value) {
    EXPECT_TRUE(bool(*result.value));
    EXPECT_EQ(7, **result.value);
    EXPECT_EQ("7", omni::ryml::as_json(*result.value));
  }
}

// Custom optional wrappers follow the same sequence omission rule.
TEST(customization_mapping, omits_empty_optional_sequence_elements) {
  const std::vector<customization_mapping::optional_value<int>> values{
    {true, 7},
    {false, 0},
    {true, 0},
  };

  EXPECT_TRUE(same_document("[7, 0]", omni::ryml::as_json(values)));
}

// The wrapper's value_type exposes its contained record for discovery.
TEST(customization_mapping, optional_record_uses_discovered_value_type) {
  const std::string document = R"({"item": {"value": 7}})";
  const auto result = omni::ryml::deserialize(
    omni::type<customization_mapping::optional_record>, document);

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  if (!result.value)
    return;

  EXPECT_TRUE(result.value->item);
  if (result.value->item)
    EXPECT_EQ(7, result.value->item->value);

  EXPECT_TRUE(same_document(document, omni::ryml::as_json(*result.value)));
}

// Default enum serialization uses the declared enumerator name.
TEST(customization_mapping, emits_enumerator_names) {
  EXPECT_EQ(R"("busy")",
    omni::ryml::as_json(customization_mapping::status::busy));
}

struct enum_mapping_case {
  std::string name;
  std::string input;
  omni::compat::expected<customization_mapping::status, omni::json::issue_code>
    expected;
};

class customization_enum_mapping:
    public testing::TestWithParam<enum_mapping_case> {};

/** Enum input must be string-valued and name a declared enumerator or a
 * configured alias. Quoting and explicit YAML tags distinguish strings from
 * plain numeric, boolean, and null scalars. */
TEST_P(customization_enum_mapping, accepts_only_declared_string_names) {
  const auto &c = GetParam();
  const auto tree = omni::ryml::core::parse(c.input);
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto result = omni::ryml::map_by_schema(
    omni::type<customization_mapping::status>, *tree);

  EXPECT_EQ(bool(c.expected), bool(result.value));
  if (c.expected) {
    if (result.value) {
      EXPECT_EQ(*c.expected, *result.value);
    }

    EXPECT_TRUE(result.diagnostics.issues.empty());
  } else {
    const auto &issues = result.diagnostics.issues;
    EXPECT_EQ(1U, issues.size());
    if (1U == issues.size()) {
      EXPECT_EQ(c.expected.error(), issues.front().reason);
      EXPECT_EQ(omni::json::value_kind::string, issues.front().expected);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(strings,
  customization_enum_mapping,
  testing::ValuesIn(std::vector<enum_mapping_case>{
    {"json", R"("ready")", customization_mapping::status::ready},
    {"yaml", "busy", customization_mapping::status::busy},
    {"quoted_null", R"("null")", customization_mapping::status::null},
    {"tagged_string", "!!str null", customization_mapping::status::null},
    {
      "unknown_name",
      R"("missing")",
      omni::compat::unexpected<omni::json::issue_code>{
        omni::json::issue_code::invalid_scalar,
      },
    },
    {
      "quoted_number",
      R"("7")",
      omni::compat::unexpected<omni::json::issue_code>{
        omni::json::issue_code::invalid_scalar,
      },
    },
    {
      "number",
      "7",
      omni::compat::unexpected<omni::json::issue_code>{
        omni::json::issue_code::unexpected_kind,
      },
    },
    {
      "boolean",
      "true",
      omni::compat::unexpected<omni::json::issue_code>{
        omni::json::issue_code::unexpected_kind,
      },
    },
    {
      "null",
      "null",
      omni::compat::unexpected<omni::json::issue_code>{
        omni::json::issue_code::unexpected_kind,
      },
    },
    {
      "mapping",
      "{}",
      omni::compat::unexpected<omni::json::issue_code>{
        omni::json::issue_code::unexpected_kind,
      },
    },
    {
      "sequence",
      "[]",
      omni::compat::unexpected<omni::json::issue_code>{
        omni::json::issue_code::unexpected_kind,
      },
    },
  }),

  [](const testing::TestParamInfo<enum_mapping_case> &info) {
    return info.param.name;
  });

} // namespace

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif

// TODO: Define global default-config customization. For now, bind local
// configuration with tdm_config or pass it explicitly to core functions.

// TODO: [high] Add cases for records with ryml::Tree data members, retaining
// subtrees whose value types are not known at compile time.
// Probably use OMNI_SKIP_METADATA(::ryml::Tree), guarded by OMNI_TOOL_RUN,
// to avoid reflecting the backend implementation.
// TODO: [high] Add an extensible-record case with three differently typed data
// members (e.g. string, int, bool) and an additional_properties map. Map known
// object keys to their members and collect the remaining entries in
// std::map<std::string, ryml::Tree> additional_properties. Also cover a
// string-valued map when all additional values are strings.

// TODO: [high] Define representations for non-string keys and which range
// constraints can be expressed by JSON Schema. uniqueItems compares whole
// entries, not just keys. Ordered ranges can sort during insertion without
// requiring sorted input.
