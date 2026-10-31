/** Executable JSON/YAML mapping guide; these examples are C++11-compatible.
 * Search for TEST(suite, topic) to find an entry point. The contents follow
 * source order; topics may have dedicated model namespaces.
 *
 * TODO: Define global default-settings customization. For now, bind local
 * settings with transform_settings or pass them explicitly to core functions.
 *
 * Basics
 *   basics.deserialize_record - default mapping interface and its result
 *   basics.serialize_record - choose JSON or YAML output
 *   basics.scalar_and_sequence_roots - documents need not be objects
 *   basics.transform_settings - customize settings for an individual call
 *   basics.diagnostics - value availability, errors, and warnings
 *
 * Settings and policies
 *   settings.core_functions - pass settings and input explicitly
 *   settings.unknown_fields - reject, ignore, or warn about unknown fields
 *   settings.duplicate_fields - choose a repeated field occurrence
 *   settings.error_limit - collect errors before stopping
 *   settings.warning_limit - fail when the warning limit is exceeded
 *   settings.missing_fields - recover missing fields where applicable
 *   settings.invalid_fields - invalid input is not a missing field
 *
 * Diagnostics
 *   diagnostics.inspect_diagnostics - issue codes, values, and paths
 *   diagnostics.render_diagnostics - render retained issues on demand
 *   diagnostics.transform_diagnostics - adapt diagnostics, preserve the value
 *   diagnostics.valid_values_with_warnings - success can retain warnings
 *
 * Supported mappings
 *   mappings.nested_records - discover nested record types
 *   mappings.sequences - vectors of records and nested vectors
 *   mappings.optional_fields - missing, null, present, and invalid values
 *   mappings.serialize_optional_fields - empty optionals become null
 *   mappings.optional_sequence_elements - omit empty optional elements
 *   TODO: Associative mappings are shown disabled.
 *
 * Advanced
 *   advanced.borrowed_and_owned_input - tree and diagnostic lifetimes
 */

#include <gtest/gtest.h>

// Mapping callables, explicit core functions, settings, and diagnostics.
#include <omnirefl/serialization/ryml.hpp>

#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

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

} // namespace basic_mapping

/** The RapidYAML-backed interface accepts JSON and YAML input. deserialize is
 * a callable with default settings already bound: pass the requested type and
 * input text to map it into a C++ value. See `basics.transform_settings` for
 * local customization and `settings.core_functions` for explicit settings.
 *
 * The result contains an optional value and retained diagnostics; a value is
 * available only on success. Diagnostic messages are rendered on demand; see
 * `diagnostics.render_diagnostics`.
 */
TEST(basics, deserialize_record) {
  const auto result = omni::ryml::deserialize(
    omni::type_t<basic_mapping::service>{},
    R"({
      "name": "gateway",
      "upstream": {
        "host": "api.example.com",
        "port": 443
      },
      "tags": ["public", "https"]
    })");

  EXPECT_TRUE(result.value.has_value());
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_EQ("gateway", result.value->name);
    EXPECT_EQ("api.example.com", result.value->upstream.host);
    EXPECT_EQ(443U, result.value->upstream.port);
    const std::vector<std::string> expected_tags{"public", "https"};
    EXPECT_EQ(expected_tags, result.value->tags);
  }
}

/** as_json and as_yaml select the output format for the same value. deserialize
 * accepts either input format; the settings control mapping, not that choice.
 */
TEST(basics, serialize_record) {
  const basic_mapping::service source{
    "gateway", {"api.example.com", 443}, {"public", "https"}};

  const std::string expected_json = R"({
    "name": "gateway",
    "upstream": {"host": "api.example.com", "port": 443},
    "tags": ["public", "https"]
  })";

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
  EXPECT_TRUE(same_document(
    expected_json, omni::ryml::as_json(source)));
  EXPECT_TRUE(same_document(
    expected_yaml, omni::ryml::as_yaml(source)));
}

// Supported scalars and vectors can be complete documents, without a record.
TEST(basics, scalar_and_sequence_roots) {
  EXPECT_EQ("42", omni::ryml::as_json(42));
  const auto scalar = omni::ryml::deserialize(omni::type_t<int>{}, "42");
  EXPECT_TRUE(scalar.value);
  if (scalar.value) {
    EXPECT_EQ(42, *scalar.value);
  }

  const std::vector<int> values{7, 42};
  EXPECT_TRUE(same_document(
    "[7, 42]", omni::ryml::as_json(values)));
  EXPECT_TRUE(same_document(
    "- 7\n- 42\n", omni::ryml::as_yaml(values)));
  const auto sequence = omni::ryml::deserialize(
    omni::type_t<std::vector<int>>{}, "[7, 42]");

  EXPECT_TRUE(sequence.value);
  if (sequence.value) {
    EXPECT_EQ(values, *sequence.value);
  }
}

namespace policy_examples {

struct measurement { int value; };

struct two_readings {
  int first;
  int second;
};

struct initialized {
  int value = 7; // Reflection exposes this literal field default.
};

} // namespace policy_examples

/** transform_settings returns a new callable with modified settings. The source
 * callable is unchanged. A capturing lambda can supply values known at runtime.
 */
TEST(basics, transform_settings) {
  const std::uint16_t limit = 2;
  const auto load = omni::ryml::deserialize.transform_settings(
    [limit](omni::ryml::settings s) {
      s.error_policy.error_limit = limit;
      s.error_policy.unknown_fields = omni::json::unknown_policy::allow;
      return s;
    });

  const auto result = load(omni::type_t<policy_examples::measurement>{},
    R"({"value": 7, "trace": true})");

  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_EQ(7, result.value->value);
  }
  EXPECT_TRUE(result.diagnostics.issues.empty());
  EXPECT_EQ(2, load.settings.error_policy.error_limit);
  EXPECT_EQ(0, omni::ryml::deserialize.settings.error_policy.error_limit);
  EXPECT_EQ(omni::json::unknown_policy::reject,
    omni::ryml::deserialize.settings.error_policy.unknown_fields);
}

/** An error prevents returning a value; warnings can accompany a valid value.
 * with_diagnostics retains both channels, unlike expected<T, Diagnostics>.
 * See `diagnostics.inspect_diagnostics` and
 * `diagnostics.valid_values_with_warnings` for their contents;
 * `diagnostics.render_diagnostics` shows message rendering.
 */
TEST(basics, diagnostics) {
  const auto result = omni::ryml::deserialize(
    omni::type_t<policy_examples::measurement>{}, R"({"value": "bad"})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  for (const auto &issue : result.diagnostics.issues) {
    EXPECT_EQ(omni::json::issue_code::invalid_scalar, issue.reason);
    EXPECT_FALSE(issue.warning);
  }
}

// The core operation takes settings explicitly; no bound callable is needed.
TEST(settings, core_functions) {
  omni::ryml::settings options{};
  options.error_policy.unknown_fields = omni::json::unknown_policy::allow;
  const auto result = omni::ryml::core::deserialize(options,
    omni::type_t<policy_examples::measurement>{},
    R"({"value": 7, "trace": true})");

  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_EQ(7, result.value->value);
  }
  EXPECT_TRUE(result.diagnostics.issues.empty());
}

/** Unknown fields are rejected by default. allow ignores them; allow | warn
 * retains a warning. warn alone does not allow an unknown field.
 */
TEST(settings, unknown_fields) {
  using omni::json::unknown_policy;
  const auto load = omni::ryml::deserialize.transform_settings(
    [](omni::ryml::settings s) {
      s.error_policy.unknown_fields =
        unknown_policy::allow | unknown_policy::warn;
      return s;
    });

  const auto result = load(omni::type_t<policy_examples::measurement>{},
    R"({"value": 7, "trace": true})");

  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_EQ(7, result.value->value);
  }
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  for (const auto &issue : result.diagnostics.issues) {
    EXPECT_EQ(omni::json::issue_code::unknown_field, issue.reason);
    EXPECT_TRUE(issue.warning);
  }
}

/** Duplicate fields are rejected by default. allow selects the first
 * occurrence; last selects the last and implies allow. Add warn to report them
 * when accepted. Discarded occurrences are not mapped; an invalid selected
 * occurrence does not fall back to a different occurrence.
 */
TEST(settings, duplicate_fields) {
  using omni::json::duplicate_policy;
  const auto load = omni::ryml::deserialize.transform_settings(
    [](omni::ryml::settings s) {
      s.error_policy.duplicates =
        duplicate_policy::last | duplicate_policy::warn;
      return s;
    });

  const auto result = load(omni::type_t<policy_examples::measurement>{},
    R"({"value": 7, "value": 42})");

  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_EQ(42, result.value->value);
  }
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  for (const auto &issue : result.diagnostics.issues) {
    EXPECT_EQ(omni::json::issue_code::duplicate_field, issue.reason);
    EXPECT_TRUE(issue.warning);
  }
}

/** error_limit counts errors that may be passed before stopping on the next.
 * Zero stops at the first error; one permits collecting two. Warnings do not
 * consume this budget, and increasing it never exposes an invalid value.
 */
TEST(settings, error_limit) {
  const auto load = omni::ryml::deserialize.transform_settings(
    [](omni::ryml::settings s) {
      s.error_policy.error_limit = 1;
      return s;
    });

  const auto result = load(omni::type_t<policy_examples::two_readings>{},
    R"({"first": "bad", "second": "bad"})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(2U, result.diagnostics.issues.size());
  EXPECT_TRUE(result.diagnostics.stopped());
}

/** warning_limit is optional: unset means unlimited, zero fails on the first
 * warning. Exceeding it adds an error and prevents returning a value.
 */
TEST(settings, warning_limit) {
  const auto load = omni::ryml::deserialize.transform_settings(
    [](omni::ryml::settings s) {
      s.error_policy.unknown_fields = omni::json::unknown_policy::allow
        | omni::json::unknown_policy::warn;
      s.error_policy.warning_limit = 0;
      return s;
    });

  const auto result = load(omni::type_t<policy_examples::measurement>{},
    R"({"value": 7, "trace": true})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(2U, result.diagnostics.issues.size());
  if (2U == result.diagnostics.issues.size()) {
    EXPECT_TRUE(result.diagnostics.issues.front().warning);
    EXPECT_EQ(omni::json::issue_code::warning_limit_exceeded,
      result.diagnostics.issues.back().reason);
    EXPECT_FALSE(result.diagnostics.issues.back().warning);
  }
}

/** Missing required fields are errors by default. field_defaults recovers
 * exposed defaults where applicable; default_initializable uses T{} where
 * supported. When combined, an exposed field default takes precedence.
 * Successful recovery does not itself add a warning.
 */
TEST(settings, missing_fields) {
  const auto load = omni::ryml::deserialize.transform_settings(
    [](omni::ryml::settings s) {
      s.recovery = omni::json::recovery_policy::field_defaults
        | omni::json::recovery_policy::default_initializable;
      return s;
    });

  const auto result = load(omni::type_t<policy_examples::initialized>{}, "{}");
  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_EQ(7, result.value->value);
  }
  EXPECT_TRUE(result.diagnostics.issues.empty());

  const auto strict = omni::ryml::deserialize(
    omni::type_t<policy_examples::initialized>{}, "{}");

  EXPECT_FALSE(strict.value);
  EXPECT_EQ(1U, strict.diagnostics.issues.size());
  for (const auto &issue : strict.diagnostics.issues)
    EXPECT_EQ(omni::json::issue_code::missing_field, issue.reason);
}

// Missing-field recovery does not accept an invalid value or an explicit null.
TEST(settings, invalid_fields) {
  const auto load = omni::ryml::deserialize.transform_settings(
    [](omni::ryml::settings s) {
      s.recovery = omni::json::recovery_policy::field_defaults;
      return s;
    });

  const std::vector<std::string> documents{
    R"({"value": "bad"})", R"({"value": null})"};

  for (const auto &document : documents) {
    SCOPED_TRACE(document);
    const auto result = load(
      omni::type_t<policy_examples::initialized>{}, document);

    EXPECT_FALSE(result.value);
    EXPECT_EQ(1U, result.diagnostics.issues.size());
    for (const auto &issue : result.diagnostics.issues)
      EXPECT_EQ(omni::json::issue_code::invalid_scalar, issue.reason);
  }
}

/** Issues retain the failed value and a path of field names/sequence indices.
 * Their views refer into the diagnostic tree; keep it alive and unchanged.
 */
TEST(diagnostics, inspect_diagnostics) {
  const auto result = omni::ryml::deserialize(
    omni::type_t<policy_examples::measurement>{}, R"({"value": "bad"})");

  EXPECT_EQ(1U, result.diagnostics.issues.size());
  for (const auto &issue : result.diagnostics.issues) {
    EXPECT_EQ(omni::json::issue_code::invalid_scalar, issue.reason);
    EXPECT_EQ(omni::json::value_kind::integer, issue.expected);
    EXPECT_EQ(omni::json::value_kind::scalar, issue.actual);
    EXPECT_EQ("bad", issue.value);
    EXPECT_EQ(1U, issue.path.size());
    for (const auto &segment : issue.path) {
      EXPECT_EQ("value", segment.field);
      EXPECT_FALSE(segment.index);
    }
  }
}

/** Rendering is optional and does not consume the retained diagnostics.
 * Messages use JSON Pointer paths, with <root> for a root-level issue.
 */
TEST(diagnostics, render_diagnostics) {
  const auto result = omni::ryml::deserialize(
    omni::type_t<policy_examples::measurement>{}, R"({"value": "bad"})");

  const std::string message =
    omni::ryml::render_diagnostics(result.diagnostics);
  EXPECT_EQ("/value: \"bad\" is not an integer", message);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
}

/** transform_diagnostics returns a result with adapted diagnostics, preserving
 * its optional value. The transform runs for successes and failures alike.
 */
TEST(diagnostics, transform_diagnostics) {
  const auto result = omni::ryml::deserialize(
    omni::type_t<policy_examples::measurement>{}, R"({"value": 7})")
    .transform_diagnostics(omni::ryml::render_diagnostics);

  EXPECT_EQ("", result.diagnostics);
  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_EQ(7, result.value->value);
  }
}

// Accepted input conditions can produce warnings alongside a valid value.
TEST(diagnostics, valid_values_with_warnings) {
  const auto load = omni::ryml::deserialize.transform_settings(
    [](omni::ryml::settings s) {
      s.error_policy.duplicates = omni::json::duplicate_policy::allow
        | omni::json::duplicate_policy::warn;
      return s;
    });

  const auto result = load(omni::type_t<policy_examples::measurement>{},
    R"({"value": 7, "value": 42})");

  EXPECT_TRUE(result.value);
  if (result.value) {
    EXPECT_EQ(7, result.value->value);
  }
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  for (const auto &issue : result.diagnostics.issues)
    EXPECT_TRUE(issue.warning);
  EXPECT_EQ("/value: warning: duplicate field",
    omni::ryml::render_diagnostics(result.diagnostics));
}

// The outer record's metadata discovers its nested record dependencies.
TEST(mappings, nested_records) {
  const auto result = omni::ryml::deserialize(
    omni::type_t<basic_mapping::service>{},
    R"({"name": "gateway", "upstream": {"host": "localhost", "port": 8080},
        "tags": []})");

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
TEST(mappings, sequences) {
  const auto result = omni::ryml::deserialize(
    omni::type_t<sequence_mapping::batch>{},
    R"({"readings": [{"value": 7}, {"value": 42}],
        "groups": [[1], [], [2, 3]]})");

  EXPECT_TRUE(result.value);
  if (result.value) {
    std::vector<int> values;
    for (const auto &reading : result.value->readings)
      values.push_back(reading.value);
    const std::vector<int> expected_values{7, 42};
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
TEST(mappings, optional_fields) {
  const std::vector<std::string> absent{"{}", R"({"value": null})"};
  for (const auto &document : absent) {
    SCOPED_TRACE(document);
    const auto result = omni::ryml::deserialize(
      omni::type_t<optional_mapping::measurement>{}, document);

    EXPECT_TRUE(result.value);
    if (result.value) {
      EXPECT_FALSE(result.value->value);
    }
  }

  const auto present = omni::ryml::deserialize(
    omni::type_t<optional_mapping::measurement>{}, R"({"value": 7})");

  EXPECT_TRUE(present.value);
  if (present.value) {
    EXPECT_TRUE(present.value->value);
    if (present.value->value) {
      EXPECT_EQ(7, *present.value->value);
    }
  }

  const auto invalid = omni::ryml::deserialize(
    omni::type_t<optional_mapping::measurement>{}, R"({"value": "bad"})");

  EXPECT_FALSE(invalid.value);
}

// Empty optional fields emit null; populated fields emit their value.
TEST(mappings, serialize_optional_fields) {
  const optional_mapping::measurement empty{omni::compat::nullopt};
  EXPECT_TRUE(same_document(
    R"({"value": null})", omni::ryml::as_json(empty)));
  EXPECT_TRUE(same_document(
    "value: null", omni::ryml::as_yaml(empty)));

  const optional_mapping::measurement present{7};
  EXPECT_TRUE(same_document(
    R"({"value": 7})", omni::ryml::as_json(present)));
}

// Empty optional sequence elements are omitted. Remaining elements keep their
// order, so their output indices can differ from their original indices.
TEST(mappings, optional_sequence_elements) {
  const std::vector<omni::compat::optional<int>> values{
    7, omni::compat::nullopt, 0, omni::compat::nullopt};

  EXPECT_TRUE(same_document(
    "[7, 0]", omni::ryml::as_json(values)));
  EXPECT_TRUE(same_document(
    "- 7\n- 0", omni::ryml::as_yaml(values)));
}

// TODO: [high] Implement associative-container mapping. Define representations
// for non-string keys and how duplicate/colliding mapped keys are handled.
// Infer key uniqueness/equivalence and ordering from container semantics, and
// investigate which constraints can be expressed by JSON Schema. uniqueItems
// compares whole entries, not just keys. Ordered containers may sort on insert
// without requiring already-sorted input.
// This example will also need <map> when enabled.
// TEST(mappings, associative_containers) {
//   const std::map<std::string, int> source{{"first", 7}, {"second", 42}};
//   const std::string json = R"({"first": 7, "second": 42})";
//   EXPECT_TRUE(same_document(
//     json, omni::ryml::as_json(source)));
//   const auto result = omni::ryml::deserialize(
//     omni::type_t<std::map<std::string, int>>{}, json);
//   EXPECT_TRUE(result.value);
//   if (result.value) {
//     EXPECT_EQ(source, *result.value);
//   }
// }

/** map_tree accepts a borrowed node or an owning tree. Borrowed diagnostics
 * require the caller's tree and any external scalar storage to stay alive and
 * unchanged. Moving the tree into map_tree lets diagnostics retain that tree.
 * Move owning diagnostics rather than copying: copying does not rebind the
 * issue views to the copied tree. deserialize owns its parsed input tree.
 */
TEST(advanced, borrowed_and_owned_input) {
  auto parsed = omni::ryml::parse(R"({"entry": {"value": "bad"}})");
  EXPECT_TRUE(parsed);
  if (parsed) {
    const auto borrowed = omni::ryml::map_tree(
      omni::type_t<policy_examples::measurement>{},
      parsed->crootref()["entry"]);

    static_assert(std::is_same<decltype(borrowed.diagnostics),
      omni::ryml::diagnostics</*OwnsTree=*/false>>::value, "borrowed tree");
    EXPECT_EQ(&*parsed, borrowed.diagnostics.tree.tree());
    EXPECT_EQ("/value: \"bad\" is not an integer",
      omni::ryml::render_diagnostics(borrowed.diagnostics));
  }

  auto owned_input = omni::ryml::parse(R"({"value": "bad"})");
  EXPECT_TRUE(owned_input);
  if (owned_input) {
    const auto owned = omni::ryml::map_tree(
      omni::type_t<policy_examples::measurement>{}, std::move(*owned_input));

    static_assert(std::is_same<decltype(owned.diagnostics),
      omni::ryml::diagnostics</*OwnsTree=*/true>>::value, "owned tree");
    EXPECT_EQ("/value: \"bad\" is not an integer",
      omni::ryml::render_diagnostics(owned.diagnostics));
  }
}

// Normalize either format to JSON text; report parse failures as test failures.
testing::AssertionResult same_document(std::string expected,
  std::string actual) {
  auto expected_tree = omni::ryml::parse(std::move(expected));
  if (!expected_tree)
    return testing::AssertionFailure() << "Invalid expected document: "
      << expected_tree.error();

  auto actual_tree = omni::ryml::parse(std::move(actual));
  if (!actual_tree)
    return testing::AssertionFailure() << "Invalid output: "
      << actual_tree.error();

  // Parsed JSON retains multi-line flow layout; normalize it before emission.
  for (auto *tree : {&*expected_tree, &*actual_tree}) {
    tree->rootref().set_style_conditionally(
      /*type_mask=*/::ryml::FLOW_ML,
      /*rem_style_flags=*/::ryml::FLOW_ML,
      /*add_style_flags=*/::ryml::FLOW_SL,
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
