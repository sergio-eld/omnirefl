/** Executable JSON/YAML mapping guide.
 * Examples use C++20 syntax for simplicity. Equivalent implementations are
 * possible through Omnirefl's C++11-compatible interfaces.
 *
 * Search for TEST(suite, topic) to find an entry point. The contents follow
 * source order; topics may have dedicated model namespaces.
 *
 * TODO: Define global default-config customization. For now, bind local
 * configuration with transform_config or pass it explicitly to core functions.
 *
 * Basics
 *   basics.deserialize_record - default mapping interface and its result
 *   basics.serialize_record - choose JSON or YAML output
 *   basics.scalar_and_sequence_roots - documents need not be objects
 *   basics.transform_config - customize config for an individual call
 *   basics.diagnostics - value availability, errors, and warnings
 *
 * Config and policies
 *   config.core_functions - pass config and input explicitly
 *   config.unknown_fields
 *     reject, ignore, or warn about unknown fields
 *   config.duplicate_fields - choose a repeated field occurrence
 *   config.error_limit - collect errors before stopping
 *   config.warning_limit - fail when the warning limit is exceeded
 *   config.missing_fields - recover missing fields where applicable
 *   config.invalid_fields - invalid input is not a missing field
 *
 * Diagnostics
 *   diagnostics.inspect_diagnostics - issue codes, values, and paths
 *   diagnostics.render_diagnostics - render retained issues on demand
 *   diagnostics.transform_diagnostics - adapt diagnostics, preserve the value
 *   diagnostics.valid_values_with_warnings
 *     success can retain warnings
 *
 * Supported mappings
 *   mappings.nested_records - discover nested record types
 *   mappings.sequences - vectors of records and nested vectors
 *   mappings.optional_fields
 *     missing, null, present, and invalid values
 *   mappings.serialize_optional_fields - empty optionals become null
 *   mappings.optional_sequence_elements - omit empty optional elements
 *   TODO: Associative mappings are shown disabled.
 *
 * Advanced
 *   advanced.tree_ownership - choose owning or borrowed string storage
 *   advanced.borrowed_and_owned_input - tree and diagnostic lifetimes
 *
 * Customization examples (experimental)
 *   customization_mapping.default_and_custom_adapters_round_trip
 *     select scalar, container, record, and custom mappings
 *   customization_mapping.custom_container_emplacement
 *     customize insertion without another mapping adapter
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

// TODO: [high] Add cases for records with ryml::Tree data members, retaining
// subtrees whose value types are not known at compile time.
// Probably use OMNI_SKIP_METADATA(::ryml::Tree), guarded by OMNI_TOOL_RUN,
// to avoid reflecting the backend implementation.
// TODO: [high] Add an extensible-record case with three differently typed data
// members (e.g. string, int, bool) and an additional_properties map. Map known
// object keys to their members and collect the remaining entries in
// std::map<std::string, ryml::Tree> additional_properties. Also cover a
// string-valued map when all additional values are strings.

#include <gtest/gtest.h>

#include <omnirefl/functional.hpp>

// Mapping callables, explicit core functions, config, and diagnostics.
#include <omnirefl/serialization/ryml.hpp>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <set>
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
 * a callable with default config already bound: pass the requested type and
 * input text to map it into a C++ value. See `basics.transform_config` for
 * local customization and `config.core_functions` for explicit config.
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
 * accepts either input format; the config control mapping, not that choice.
 */
TEST(basics, serialize_record) {
  const basic_mapping::service source{
    "gateway",
    {"api.example.com", 443},
    {"public", "https"},
  };

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

/** transform_config returns a new callable with modified config. The source
 * callable is unchanged. A capturing lambda can supply values known at runtime.
 */
TEST(basics, transform_config) {
  const std::uint16_t limit = 2;
  const auto load = omni::ryml::deserialize.transform_config(
    [limit](omni::ryml::config s) {
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
  EXPECT_EQ(2, load.config.error_policy.error_limit);
  EXPECT_EQ(0, omni::ryml::deserialize.config.error_policy.error_limit);
  EXPECT_EQ(omni::json::unknown_policy::reject,
    omni::ryml::deserialize.config.error_policy.unknown_fields);
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

// The core operation takes configuration explicitly instead of using a bound
// callable.
TEST(config, core_functions) {
  omni::ryml::config options{};
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
TEST(config, unknown_fields) {
  using omni::json::unknown_policy;
  const auto load = omni::ryml::deserialize.transform_config(
    [](omni::ryml::config s) {
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
TEST(config, duplicate_fields) {
  using omni::json::duplicate_policy;
  const auto load = omni::ryml::deserialize.transform_config(
    [](omni::ryml::config s) {
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
TEST(config, error_limit) {
  const auto load = omni::ryml::deserialize.transform_config(
    [](omni::ryml::config s) {
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
TEST(config, warning_limit) {
  const auto load = omni::ryml::deserialize.transform_config(
    [](omni::ryml::config s) {
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
TEST(config, missing_fields) {
  const auto load = omni::ryml::deserialize.transform_config(
    [](omni::ryml::config s) {
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
TEST(config, invalid_fields) {
  const auto load = omni::ryml::deserialize.transform_config(
    [](omni::ryml::config s) {
      s.recovery = omni::json::recovery_policy::field_defaults;
      return s;
    });

  const std::vector<std::string> documents{
    R"({"value": "bad"})",
    R"({"value": null})",
  };

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
  const auto load = omni::ryml::deserialize.transform_config(
    [](omni::ryml::config s) {
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
    7, omni::compat::nullopt, 0, omni::compat::nullopt,
  };

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

/** map_tree accepts a borrowed node or an owning tree. Borrowed diagnostics
 * require the caller's tree and any external scalar storage to stay alive and
 * unchanged. Moving the tree into map_tree lets diagnostics retain that tree.
 * Move owning diagnostics rather than copying: copying does not rebind the
 * issue views to the copied tree. deserialize owns its parsed input tree.
 */
TEST(advanced, borrowed_and_owned_input) {
  auto parsed = omni::ryml::core::parse(R"({"entry": {"value": "bad"}})");
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

  auto owned_input = omni::ryml::core::parse(R"({"value": "bad"})");
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

// Only optional_record discovers this type; earlier calls cannot supply metadata.
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

  // C++11 classes with bases need a constructor for case-data initialization.
  extended_values(std::vector<int> elements = {},
    int first = 0,
    int second = 0):
      std::vector<int>(std::move(elements)), first(first), second(second) {}
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
namespace cp {

template <typename T, typename Compare, typename Allocator>
struct emplace<std::set<T, Compare, Allocator>> {
  void operator()(std::set<T, Compare, Allocator> *into, T &&value) const {
    into->insert(std::move(value));
  }
};

// Explicit specializations reuse record mapping without selecting themselves.
// Generic operators defer metadata access until the reflected scope.
template <>
struct from_type<customization_mapping::record>:
    reflected_scope::from_record {};

template <>
struct to_type<customization_mapping::record>:
    reflected_scope::to_record<> {};

// Ad hoc: inheriting standard containers is discouraged. This custom mapping
// needs the derived record's metadata as well as container iteration.
template <>
struct reflect_as_root<customization_mapping::extended_values>:
    std::true_type {};

// Record metadata describes the added fields; container adapters handle the
// flat representation. The fields occupy its final two positions.
template <>
struct from_type<customization_mapping::extended_values> {
  struct cpp11_append_member {
    ::ryml::NodeRef &node;
    const detail::mapping_context &context;

    template <typename Record, typename _F>
    void operator()(omni::data_member_binding_t<Record, _F> field) const {
      core::write_node(field.value(),
        this->node.append_child(),
        this->context);
    }
  };

  template <typename T>
  void operator()(const T &value,
    ::ryml::NodeRef node,
    const detail::mapping_context &context) const {
    mapping::from_container{}(value, node, context);
    const auto binding = omni::meta_for<T>::bind(value);
    binding.accessible_data_members()
      | fn::each(cpp11_append_member{
        /*node=*/node,
        /*context=*/context,
      });
  }
};

template <>
struct to_type<customization_mapping::extended_values> {
  struct cpp11_restore_member {
    template <typename Record, typename _F>
    typename Record::const_iterator operator()(
      typename Record::const_iterator from,
      omni::data_member_binding_t<Record, _F> field) const {
      field.set_value(*from);
      return std::next(from);
    }
  };

  template <typename T>
  node_result operator()(::ryml::ConstNodeRef node,
    T *into,
    const detail::mapping_context &context) const {
    T values;
    auto result = mapping::to_container{}(node, &values, context);
    if (!result.value)
      return result;

    const auto binding = omni::meta_for<T>::bind(*into);
    const auto members = binding.accessible_data_members();
    const auto count = std::tuple_size<decltype(members)>::value;
    if (values.size() < count) {
      auto failure = detail::make_node_accumulator(context, node);
      failure.append(std::move(result));
      failure.report(detail::make_issue(json::issue_code::out_of_range,
        json::value_kind::sequence,
        node,
        context.path));

      return std::move(failure).finish();
    }

    const auto tail = std::next(values.cbegin(), values.size() - count);
    members | fn::foldl(cpp11_restore_member{}, tail);
    into->assign(values.cbegin(), tail);
    return result;
  }
};

template <>
struct from_type<customization_mapping::integer> {
  void operator()(const customization_mapping::integer &value,
    ::ryml::NodeRef node,
    const detail::mapping_context &context) const {
    core::write_node(value.value, node, context);
  }
};

template <>
struct to_type<customization_mapping::integer> {
  node_result operator()(::ryml::ConstNodeRef node,
    customization_mapping::integer *into,
    const detail::mapping_context &context) const {
    int converted{};
    auto result = core::read_node(node, &converted, context);
    if (!result.value)
      return result;

    if (converted < 4 || 8 < converted) {
      auto failure = detail::make_node_accumulator(context, node);
      failure.append(std::move(result));
      failure.report(detail::make_issue(json::issue_code::out_of_range,
        json::value_kind::integer,
        node,
        context.path));

      return std::move(failure).finish();
    }

    into->value = converted;
    return result;
  }
};

} // namespace cp
} // namespace ryml
} // namespace omni

namespace {

/** Customizations are selected at every node, including container elements.
 * The entry points establish reflected scope; adapters never re-enter it.
 * The integer wrapper is mapped as a scalar, not a record.
 */
TEST(customization_mapping, default_and_custom_adapters_round_trip) {
  const customization_mapping::record source{
    2,
    {{7, "first"}, {8, "second"}},
    {6},
  };
  const auto document = omni::ryml::as_json(source);
  EXPECT_TRUE(same_document(R"({
    "count": 2,
    "entries": [{"value": 7, "name": "first"},
                {"value": 8, "name": "second"}],
    "reading": 6
  })", document));

  const auto result = omni::ryml::deserialize(
    omni::type_t<customization_mapping::record>{}, document);
  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
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
TEST(customization_mapping, custom_container_emplacement) {
  const auto result = omni::ryml::deserialize(
    omni::type_t<std::set<int>>{}, "[7, 4]");
  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_EQ((std::set<int>{4, 7}), *result.value);
    EXPECT_TRUE(same_document("[4, 7]",
      omni::ryml::as_json(*result.value)));
  }
}

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
  EXPECT_TRUE(same_document(c.document,
    omni::ryml::as_json(c.source)));

  const auto result = omni::ryml::deserialize(
    omni::type_t<customization_mapping::extended_values>{}, c.document);
  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
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

  const omni::ryml::config options{};
  const omni::ryml::detail::mapping_context context{
    /*options=*/options,
    /*path=*/{},
    /*error_count=*/0,
    /*warning_count=*/0,
    /*borrow_strings=*/false,
  };
  auto into = c.initial;
  const auto mapped = omni::ryml::core::map_tree(
    parsed->crootref(), &into, context);

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
      omni::json::issue_code::invalid_scalar,
      "/3",
    },
    {
      "invalid_prefix",
      R"(["bad", 4, 8])",
      {{}, 2, 7},
      omni::json::issue_code::invalid_scalar,
      "/0",
    },
  }),
  [](const testing::TestParamInfo<invalid_extended_container_case> &info) {
    return info.param.name;
  });

class customization_mapping_limits:
    public testing::TestWithParam<std::uint16_t> {};

// Adapters contribute issues; the core owns counting and error-limit checks.
TEST_P(customization_mapping_limits,
  collects_default_and_custom_conversion_errors) {
  omni::ryml::config options{};
  options.error_policy.error_limit = GetParam();
  const auto result = omni::ryml::core::deserialize(options,
    omni::type_t<customization_mapping::record>{},
    R"({
      "count": 2,
      "entries": [{"value": "bad", "name": "first"}],
      "reading": 9
    })");

  EXPECT_FALSE(result.value);
  const auto &issues = result.diagnostics.issues;
  const std::size_t expected_errors = 0 == GetParam() ? 1 : 2;
  EXPECT_EQ(expected_errors, issues.size());
  if (!issues.empty()) {
    EXPECT_EQ(omni::json::issue_code::invalid_scalar, issues[0].reason);
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

TEST_P(customization_optional_mapping, maps_nullable_values) {
  const auto &c = GetParam();
  auto parsed = omni::ryml::core::parse(c.document);
  EXPECT_TRUE(parsed);
  if (!parsed)
    return;

  const omni::ryml::config options{};
  const omni::ryml::detail::mapping_context context{
    /*options=*/options,
    /*path=*/{},
    /*error_count=*/0,
    /*warning_count=*/0,
    /*borrow_strings=*/false,
  };
  auto into = c.initial;
  const auto mapped = omni::ryml::core::map_tree(
    parsed->crootref(), &into, context);

  EXPECT_EQ(bool(c.expected), bool(mapped.value));
  if (c.expected) {
    EXPECT_EQ(*c.expected, into);
    EXPECT_TRUE(mapped.diagnostics.issues.empty());

    EXPECT_TRUE(same_document(c.document,
      omni::ryml::as_json(into)));
  } else {
    EXPECT_EQ(c.initial, into);
    EXPECT_EQ(1U, mapped.diagnostics.issues.size());
    if (1U == mapped.diagnostics.issues.size()) {
      EXPECT_EQ(c.expected.error(), mapped.diagnostics.issues.front().reason);
    }
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
        omni::json::issue_code::invalid_scalar,
      },
    },
  }),
  [](const testing::TestParamInfo<optional_mapping_case> &info) {
    return info.param.name;
  });

TEST(customization_mapping, user_optional_operations_reuse_mapping_adapters) {
  const auto result = omni::ryml::deserialize(
    omni::type_t<customization_mapping::optional_value<int>>{}, "7");
  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_TRUE(bool(*result.value));
    EXPECT_EQ(7, **result.value);
    EXPECT_EQ("7", omni::ryml::as_json(*result.value));
  }
}

TEST(customization_mapping, omits_empty_optional_sequence_elements) {
  const std::vector<customization_mapping::optional_value<int>> values{
    {true, 7},
    {false, 0},
    {true, 0},
  };
  EXPECT_TRUE(same_document("[7, 0]",
    omni::ryml::as_json(values)));
}

TEST(customization_mapping, optional_record_uses_discovered_value_type) {
  const std::string document = R"({"item": {"value": 7}})";
  const auto result = omni::ryml::deserialize(
    omni::type_t<customization_mapping::optional_record>{}, document);
  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (!result.value)
    return;

  EXPECT_TRUE(result.value->item);
  if (result.value->item) {
    EXPECT_EQ(7, result.value->item->value);
  }

  EXPECT_TRUE(same_document(document,
    omni::ryml::as_json(*result.value)));
}

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

TEST_P(customization_enum_mapping, accepts_only_declared_string_names) {
  const auto &c = GetParam();
  const auto result = omni::ryml::deserialize(
    omni::type_t<customization_mapping::status>{}, c.input);
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
