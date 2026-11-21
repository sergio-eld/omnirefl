/** Runtime comparison of schema traversal and handwritten document lookup.
 * Both readers use default policies and return structured diagnostics.
 * Parsing, correctness checks, and source construction precede timed loops.
 * Both writers borrow source strings and use the same JSON emission function.
 * C++20 keeps the benchmark's value comparisons and registration concise. */

#include <omnirefl/serialization/ryml.hpp>

#include <benchmark/benchmark.h>
// RapidYAML: third-party JSON/YAML backend shared by both implementations.
#include <ryml.hpp>
#include <c4/format.hpp>
#include <c4/std/string.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace mapping_comparison {

struct flat_record {
  unsigned id;
  std::string name;
  unsigned retries;
  bool enabled;
  double threshold;
  int timeout;

  bool operator==(const flat_record &) const = default;
};

struct nested_record {
  std::string label;
  flat_record settings;
  unsigned version;

  bool operator==(const nested_record &) const = default;
};

struct deep_record {
  unsigned revision;
  nested_record primary;
  nested_record backup;

  bool operator==(const deep_record &) const = default;
};

struct sequence_record {
  std::string name;
  std::vector<nested_record> services;
  bool enabled;

  bool operator==(const sequence_record &) const = default;
};

// These helpers share only node validation, conversion, and issue construction.
// No handwritten reader/writer uses reflection, cp lookup, or record mapping.
// Structural errors precede missing fields, then values follow declaration
// order, just as they do under the default map_by_schema configuration.
auto check_members(::ryml::ConstNodeRef object,
  std::initializer_list<c4::csubstr> names)
  -> omni::compat::expected<omni::compat::monostate, omni::ryml::issue> {
  if (auto checked = omni::ryml::core::expect_object_node(object); !checked)
    return omni::compat::unexpected{std::move(checked.error())};

  // The benchmark records have at most six fields. Remember presence while
  // checking structural errors, leaving one document lookup per value below.
  std::uint64_t seen = 0;
  for (const auto child : object.children()) {
    const auto found = std::find(names.begin(), names.end(), child.key());
    if (found == names.end())
      return omni::compat::unexpected{omni::ryml::core::make_issue(
        omni::json::issue_code::unknown_field,
        omni::json::value_kind::unknown, child)};

    const auto bit = std::uint64_t{1} << (found - names.begin());
    if (seen & bit)
      return omni::compat::unexpected{omni::ryml::core::make_issue(
        omni::json::issue_code::duplicate_field,
        omni::json::value_kind::unknown, child)};

    seen |= bit;
  }

  for (std::size_t i = 0; i < names.size(); ++i) {
    if (seen & (std::uint64_t{1} << i))
      continue;

    auto missing = omni::ryml::core::make_issue(
      omni::json::issue_code::missing_field,
      omni::json::value_kind::unknown, ::ryml::ConstNodeRef{});

    missing.path = omni::ryml::core::node_path(object);
    missing.path.push_back({names.begin()[i], omni::compat::nullopt});
    return omni::compat::unexpected{std::move(missing)};
  }

  return omni::compat::monostate{};
}

template <typename T>
auto read_scalar(::ryml::ConstNodeRef node)
  -> omni::compat::expected<T, omni::ryml::issue> {
  if constexpr (std::is_same_v<T, std::string>) {
    if (auto checked = omni::ryml::core::expect_string_node(node); !checked)
      return omni::compat::unexpected{std::move(checked.error())};

    return node.val().empty()
      ? std::string{} : std::string{node.val().str, node.val().len};
  } else if constexpr (std::is_same_v<T, bool>) {
    if (auto checked = omni::ryml::core::expect_boolean_node(node); !checked)
      return omni::compat::unexpected{std::move(checked.error())};

    return *omni::ryml::core::parse_boolean(
      {node.val().str, node.val().len});
  } else {
    const auto checked = std::is_integral_v<T>
      ? omni::ryml::core::expect_integer_node(node)
      : omni::ryml::core::expect_number_node(node);

    if (!checked)
      return omni::compat::unexpected{checked.error()};

    T value{};
    const bool converted = omni::compat::invoke([&node, &value] {
      if constexpr (std::is_integral_v<T>) {
        auto checked_value = c4::fmt::overflow_checked(value);
        return c4::from_chars(node.val(), &checked_value);
      } else
        return c4::from_chars(node.val(), &value);
    });

    if (!converted)
      return omni::compat::unexpected{omni::ryml::core::make_issue(
        omni::json::issue_code::out_of_range,
        std::is_floating_point_v<T> ? omni::json::value_kind::number
          : std::is_unsigned_v<T> ? omni::json::value_kind::unsigned_integer
                                 : omni::json::value_kind::integer,
        node)};

    return value;
  }
}

auto read_manual(::ryml::ConstNodeRef node, omni::type_t<flat_record>)
  -> omni::compat::expected<flat_record, omni::ryml::issue> {
  if (auto checked = check_members(node,
        {"id", "name", "retries", "enabled", "threshold", "timeout"});
    !checked)
    return omni::compat::unexpected{std::move(checked.error())};

  auto id = read_scalar<unsigned>(node.find_child("id"));
  if (!id)
    return omni::compat::unexpected{std::move(id.error())};

  auto name = read_scalar<std::string>(node.find_child("name"));
  if (!name)
    return omni::compat::unexpected{std::move(name.error())};

  auto retries = read_scalar<unsigned>(node.find_child("retries"));
  if (!retries)
    return omni::compat::unexpected{std::move(retries.error())};

  auto enabled = read_scalar<bool>(node.find_child("enabled"));
  if (!enabled)
    return omni::compat::unexpected{std::move(enabled.error())};

  auto threshold = read_scalar<double>(node.find_child("threshold"));
  if (!threshold)
    return omni::compat::unexpected{std::move(threshold.error())};

  auto timeout = read_scalar<int>(node.find_child("timeout"));
  if (!timeout)
    return omni::compat::unexpected{std::move(timeout.error())};

  return flat_record{*id, std::move(*name), *retries, *enabled,
    *threshold, *timeout};
}

auto read_manual(::ryml::ConstNodeRef node, omni::type_t<nested_record>)
  -> omni::compat::expected<nested_record, omni::ryml::issue> {
  if (auto checked = check_members(node, {"label", "settings", "version"});
    !checked)
    return omni::compat::unexpected{std::move(checked.error())};

  auto label = read_scalar<std::string>(node.find_child("label"));
  if (!label)
    return omni::compat::unexpected{std::move(label.error())};

  auto settings = read_manual(node.find_child("settings"),
    omni::type<flat_record>);

  if (!settings)
    return omni::compat::unexpected{std::move(settings.error())};

  auto version = read_scalar<unsigned>(node.find_child("version"));
  if (!version)
    return omni::compat::unexpected{std::move(version.error())};

  return nested_record{std::move(*label), std::move(*settings), *version};
}

auto read_manual(::ryml::ConstNodeRef node, omni::type_t<deep_record>)
  -> omni::compat::expected<deep_record, omni::ryml::issue> {
  if (auto checked = check_members(node, {"revision", "primary", "backup"});
    !checked)
    return omni::compat::unexpected{std::move(checked.error())};

  auto revision = read_scalar<unsigned>(node.find_child("revision"));
  if (!revision)
    return omni::compat::unexpected{std::move(revision.error())};

  auto primary = read_manual(node.find_child("primary"),
    omni::type<nested_record>);

  if (!primary)
    return omni::compat::unexpected{std::move(primary.error())};

  auto backup = read_manual(node.find_child("backup"),
    omni::type<nested_record>);

  if (!backup)
    return omni::compat::unexpected{std::move(backup.error())};

  return deep_record{*revision, std::move(*primary), std::move(*backup)};
}

auto read_manual(::ryml::ConstNodeRef node, omni::type_t<sequence_record>)
  -> omni::compat::expected<sequence_record, omni::ryml::issue> {
  if (auto checked = check_members(node, {"name", "services", "enabled"});
    !checked)
    return omni::compat::unexpected{std::move(checked.error())};

  auto name = read_scalar<std::string>(node.find_child("name"));
  if (!name)
    return omni::compat::unexpected{std::move(name.error())};

  const auto services = node.find_child("services");
  if (auto checked = omni::ryml::core::expect_array_node(services); !checked)
    return omni::compat::unexpected{std::move(checked.error())};

  std::vector<nested_record> values;
  values.reserve(services.num_children());
  for (const auto child : services.children()) {
    auto mapped = read_manual(child, omni::type<nested_record>);
    if (!mapped)
      return omni::compat::unexpected{std::move(mapped.error())};

    values.emplace_back(std::move(*mapped));
  }

  auto enabled = read_scalar<bool>(node.find_child("enabled"));
  if (!enabled)
    return omni::compat::unexpected{std::move(enabled.error())};

  return sequence_record{std::move(*name), std::move(values), *enabled};
}

template <typename T>
auto manual_map(::ryml::ConstNodeRef node, omni::type_t<T> type)
  -> omni::ryml::with_diagnostics_t<T> {
  auto result = read_manual(node, type);
  if (!result)
    return {
      .value = omni::compat::nullopt,
      .diagnostics = {{}, {std::move(result.error())}, {}},
    };

  return {.value = std::move(*result), .diagnostics = {}};
}

// Keys are copied into the arena, and string values borrow their source,
// matching as_ref_tree, which is also used by the default as_json callable.
template <typename T>
void write_scalar(const T &value, ::ryml::NodeRef node) {
  if constexpr (std::is_same_v<T, std::string>) {
    node.set_val({value.data(), value.size()});
    node.set_val_style(::ryml::VAL_DQUO);
  } else if constexpr (std::is_same_v<T, bool>)
    node.set_serialized(c4::fmt::boolalpha(value));
  else if constexpr (std::is_floating_point_v<T>)
    node.set_serialized(c4::fmt::real(value,
      std::numeric_limits<T>::max_digits10, c4::FTOA_FLEX));
  else
    node.set_serialized(value);
}

template <typename T>
void write_member(::ryml::NodeRef object, c4::csubstr name, const T &value) {
  auto child = object.append_child();
  child.set_key_serialized(name);
  write_scalar(value, child);
}

void write_manual(const flat_record &value, ::ryml::NodeRef node) {
  node.set_map();
  write_member(node, "id", value.id);
  write_member(node, "name", value.name);
  write_member(node, "retries", value.retries);
  write_member(node, "enabled", value.enabled);
  write_member(node, "threshold", value.threshold);
  write_member(node, "timeout", value.timeout);
}

void write_manual(const nested_record &value, ::ryml::NodeRef node) {
  node.set_map();
  write_member(node, "label", value.label);
  auto settings = node.append_child();
  settings.set_key_serialized("settings");
  write_manual(value.settings, settings);
  write_member(node, "version", value.version);
}

void write_manual(const deep_record &value, ::ryml::NodeRef node) {
  node.set_map();
  write_member(node, "revision", value.revision);
  auto primary = node.append_child();
  primary.set_key_serialized("primary");
  write_manual(value.primary, primary);
  auto backup = node.append_child();
  backup.set_key_serialized("backup");
  write_manual(value.backup, backup);
}

void write_manual(const sequence_record &value, ::ryml::NodeRef node) {
  node.set_map();
  write_member(node, "name", value.name);
  auto services = node.append_child();
  services.set_key_serialized("services");
  services.set_seq();
  for (const auto &service : value.services)
    write_manual(service, services.append_child());

  write_member(node, "enabled", value.enabled);
}

// Both sides include tree construction, allocation, numeric formatting,
// emission, and destruction. Workloads contain no raw ASCII controls, so the
// default writer's external-backend repair pass is unnecessary on either side.
std::string render_json(const ::ryml::Tree &tree) {
  std::string text;
  text.reserve(tree.arena_size() + 8 * tree.size());
  ::ryml::emitrs_json(tree, &text);
  return text;
}

struct document_case {
  std::string name;
  std::string json;
  std::string error; //< Empty for valid input; otherwise the rendered error.
};

// Both sides must agree on values and every issue, not just success/failure.
template <typename T>
std::string check_read(const document_case &input, const ::ryml::Tree &tree,
  const omni::ryml::with_diagnostics_t<T> &schema,
  const omni::ryml::with_diagnostics_t<T> &manual) {
  if (schema.value != manual.value)
    return "decoded values differ";

  if (schema.value.has_value() != input.error.empty())
    return "unexpected acceptance/rejection";

  if (omni::ryml::render_diagnostics(tree, schema.diagnostics) != input.error)
    return "schema diagnostics differ from the expected error";

  if (omni::ryml::render_diagnostics(tree, manual.diagnostics) != input.error)
    return "manual diagnostics differ from the expected error";

  if (schema.diagnostics.issues.size() != manual.diagnostics.issues.size())
    return "diagnostic counts differ";

  for (std::size_t i = 0; i < schema.diagnostics.issues.size(); ++i) {
    const auto &a = schema.diagnostics.issues[i];
    const auto &b = manual.diagnostics.issues[i];
    if (a.reason != b.reason || a.expected != b.expected
      || a.actual != b.actual || a.warning != b.warning
      || a.node_id != b.node_id || a.path.size() != b.path.size())
      return "structured issues differ";

    for (std::size_t j = 0; j < a.path.size(); ++j)
      if (a.path[j].field != b.path[j].field
        || a.path[j].index != b.path[j].index)
        return "diagnostic paths differ";
  }

  return {};
}

template <typename T, bool Manual>
void read_benchmark(benchmark::State &state, const document_case &input) {
  const auto tree = ::ryml::parse_in_arena(
    c4::csubstr{input.json.data(), input.json.size()});

  const auto error = check_read(input, tree,
    omni::ryml::map_by_schema(omni::type<T>, tree),
    manual_map(tree.crootref(), omni::type<T>));

  if (!error.empty()) {
    state.SkipWithError(error.c_str());
    return;
  }

  for (auto _ : state) {
    auto result = omni::compat::invoke([&tree] {
      if constexpr (Manual)
        return manual_map(tree.crootref(), omni::type<T>);
      else
        return omni::ryml::map_by_schema(omni::type<T>, tree);
    });

    benchmark::DoNotOptimize(result);
  }

  state.SetBytesProcessed(
    state.iterations() * static_cast<std::int64_t>(input.json.size()));
}

template <typename T, bool Manual>
void write_benchmark(benchmark::State &state, const document_case &input) {
  const auto tree = ::ryml::parse_in_arena(
    c4::csubstr{input.json.data(), input.json.size()});

  const auto source = manual_map(tree.crootref(), omni::type<T>);
  const auto checked = check_read(input, tree,
    omni::ryml::map_by_schema(omni::type<T>, tree), source);

  if (!checked.empty()) {
    state.SkipWithError(checked.c_str());
    return;
  }

  ::ryml::Tree written;
  write_manual(*source.value, written.rootref());
  const auto expected = omni::ryml::as_json(*source.value);
  if (render_json(written) != expected
    || render_json(omni::ryml::as_ref_tree(*source.value)) != expected) {
    state.SkipWithError("JSON writers differ");
    return;
  }

  const auto roundtrip = omni::ryml::deserialize(omni::type<T>, expected);
  if (roundtrip.value != source.value) {
    state.SkipWithError("written JSON does not preserve the source value");
    return;
  }

  for (auto _ : state) {
    auto result = omni::compat::invoke([&source] {
      if constexpr (Manual) {
        ::ryml::Tree output;
        write_manual(*source.value, output.rootref());
        return render_json(output);
      } else
        return render_json(omni::ryml::as_ref_tree(*source.value));
    });

    benchmark::DoNotOptimize(result);
  }

  state.SetBytesProcessed(
    state.iterations() * static_cast<std::int64_t>(expected.size()));
}

std::string replace_first(std::string text,
  std::string_view from, std::string_view to) {
  const auto position = text.find(from);
  if (position != std::string::npos)
    text.replace(position, from.size(), to);

  return text;
}

std::vector<document_case> documents(std::string valid,
  std::string_view duplicate_key, std::string_view leaf_path,
  std::string_view last_leaf_path = {}) {
  auto unknown = valid;
  unknown.insert(unknown.size() - 1, R"(,"extra":0)");
  auto duplicate = valid;
  duplicate.insert(duplicate.size() - 1,
    ",\"" + std::string{duplicate_key} + "\":0");

  std::vector<document_case> result{
    {"valid", valid, {}},
    {"invalid_value", replace_first(valid, "\"timeout\":1500",
        "\"timeout\":\"bad\""),
      std::string{leaf_path} + ": expected integer, found scalar"},
    {"null", replace_first(valid, "\"timeout\":1500", "\"timeout\":null"),
      std::string{leaf_path} + ": expected integer, found null"},
    {"overflow", replace_first(valid, "\"timeout\":1500",
        "\"timeout\":999999999999999999999"),
      std::string{leaf_path} + ": \"999999999999999999999\" is out of range"},
    {"missing", replace_first(valid, ",\"timeout\":1500", ""),
      std::string{leaf_path} + ": missing field"},
    {"unknown", std::move(unknown), "/extra: unknown field"},
    {"duplicate", std::move(duplicate),
      "/" + std::string{duplicate_key} + ": duplicate field"},
    {"wrong_root", "[]", "<root>: expected object, found array"},
  };

  if (!last_leaf_path.empty()) {
    auto invalid_last = valid;
    invalid_last.replace(invalid_last.rfind("\"timeout\":1500"),
      std::string_view{"\"timeout\":1500"}.size(), "\"timeout\":\"bad\"");
    result.emplace_back("invalid_last_value", std::move(invalid_last),
      std::string{last_leaf_path} + ": expected integer, found scalar");
  }

  return result;
}

template <typename T>
bool register_comparison(const std::string &name,
  const std::vector<document_case> &inputs) {
  for (const auto &input : inputs) {
    const auto prefix = "comparison/read/" + name + "/" + input.name;
    benchmark::RegisterBenchmark((prefix + "/schema").c_str(),
      read_benchmark<T, false>, input);
    benchmark::RegisterBenchmark((prefix + "/manual").c_str(),
      read_benchmark<T, true>, input);
  }

  const auto prefix = "comparison/write/" + name;
  benchmark::RegisterBenchmark((prefix + "/schema").c_str(),
    write_benchmark<T, false>, inputs.front());
  benchmark::RegisterBenchmark((prefix + "/manual").c_str(),
    write_benchmark<T, true>, inputs.front());
  return true;
}

const std::string flat_json = R"({"id":42,"name":"gateway.example.com",)"
  R"("retries":3,"enabled":true,"threshold":0.75,"timeout":1500})";

const std::string nested_json =
  R"({"label":"primary","settings":)" + flat_json + R"(,"version":2})";

const bool registered_flat = register_comparison<flat_record>("flat",
  documents(flat_json, "id", "/timeout"));

const bool registered_nested = register_comparison<nested_record>("nested",
  documents(nested_json, "label", "/settings/timeout"));

const bool registered_deep = register_comparison<deep_record>("deep",
  documents(R"({"revision":7,"primary":)" + nested_json
      + R"(,"backup":)" + nested_json + "}",
    "revision", "/primary/settings/timeout", "/backup/settings/timeout"));

const bool registered_sequence = register_comparison<sequence_record>(
  "sequence", documents(R"({"name":"cluster","services":[)"
      + nested_json + "," + nested_json + "," + nested_json + ","
      + nested_json + R"(],"enabled":true})",
    "name", "/services/0/settings/timeout", "/services/3/settings/timeout"));

} // namespace mapping_comparison
