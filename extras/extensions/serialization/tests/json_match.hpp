#pragma once

#include <omnirefl/serialization/ryml.hpp>
#include <gtest/gtest.h>

#include <string>
#include <deque>
#include <memory>
#include <type_traits>

namespace serialization_test {

// Capture both observable forms while deserialize still owns the source tree.
// Copied field names keep paths valid after that tree is released; shared
// storage preserves those views when a case result is copied or moved.
struct collected_diagnostics: omni::ryml::diagnostics {
  std::shared_ptr<std::deque<std::string>> fields;
  std::string message;

  collected_diagnostics() = default;

  collected_diagnostics(const ::ryml::Tree &tree,
    omni::ryml::diagnostics report)
      : omni::ryml::diagnostics(std::move(report))
      , fields(std::make_shared<std::deque<std::string>>())
      , message(omni::ryml::render_diagnostics(tree, *this)) {
    for (auto &entry : this->issues) {
      for (auto &segment : entry.path) {
        const auto text = omni::ryml::detail::to_string_view(segment.field);
        this->fields->emplace_back(text.data(), text.size());
        segment.field = c4::to_csubstr(this->fields->back());
      }
    }
  }
};

struct collect_diagnostics {
  collected_diagnostics operator()(const ::ryml::Tree &tree,
    omni::ryml::diagnostics report) const {
    return {tree, std::move(report)};
  }
};

using config = omni::ryml::config_t<collect_diagnostics>;
using deserialize_type = omni::ryml::deserialize_t<config>;
const deserialize_type deserialize{config{}};

// Borrow one mapping for T while retaining normal nested-type lookup.
template <typename T, typename Mapping>
struct select_mapping {
  const Mapping &mapping;

  template <typename State>
  const Mapping &operator()(const State &, omni::type_t<T>) const {
    return this->mapping;
  }

  OMNI_CONSTRAINED_TEMPLATE((typename State, typename Other),
    (!std::is_same<T, Other>::value))
  auto operator()(const State &state, omni::type_t<Other> type) const
    -> decltype(omni::ryml::reflected_scope::fn::mapping_to_type(state, type)) {
    return omni::ryml::reflected_scope::fn::mapping_to_type(state, type);
  }
};

template <typename Mapping, typename T>
auto map_using(const omni::ryml::config_t<> &options, const Mapping &mapping,
  omni::type_t<T> type, ::ryml::ConstNodeRef node)
  -> omni::ryml::with_diagnostics_t<T> {
  const omni::ryml::map_by_schema_t<const omni::ryml::config_t<> &,
    select_mapping<T, Mapping>> map{options, {mapping}};
  return map(type, node);
}

template <typename Mapping, typename T>
auto map_using(const omni::ryml::config_t<> &options, const Mapping &mapping,
  T *into, ::ryml::ConstNodeRef node)
  -> omni::ryml::with_diagnostics_t<omni::compat::monostate> {
  const omni::ryml::map_by_schema_t<const omni::ryml::config_t<> &,
    select_mapping<T, Mapping>> map{options, {mapping}};
  return map(into, node);
}

// Both format rows use the public configurable serialization binding.
template <typename T>
std::string serialize(::ryml::EmitType_e format, const T &value) {
  return omni::ryml::serialize
    .tdm_format([format](::ryml::EmitType_e) { return format; })(value);
}

// Existing conformance cases compare rendered text as well as structured data.
struct render_collected_diagnostics {
  std::string operator()(const collected_diagnostics &report) const {
    return report.message;
  }

  std::string operator()(const std::string &message) const { return message; }
};

constexpr render_collected_diagnostics render_diagnostics{};

// Compare structure, scalar spelling and quoting, ignoring map key order and
// formatting. Quoting distinguishes JSON strings from numbers and booleans.
inline testing::AssertionResult match_json_nodes(::ryml::ConstNodeRef expected,
  ::ryml::ConstNodeRef actual, const std::string &path) {
  if (expected.is_map() != actual.is_map()
    || expected.is_seq() != actual.is_seq()
    || expected.has_val() != actual.has_val()
    || expected.num_children() != actual.num_children()) {
    return testing::AssertionFailure() << path << ": different node shapes";
  }

  if (expected.is_map() || expected.is_seq()) {
    std::size_t index = 0;
    for (const auto child : expected.children()) {
      std::string child_path = path + "/";
      if (expected.is_map()) {
        const auto key = omni::ryml::detail::to_string_view(child.key());
        child_path.append(key.data(), key.size());
      } else {
        child_path += std::to_string(index);
      }

      if (expected.is_map() && !actual.has_child(child.key()))
        return testing::AssertionFailure() << child_path << ": missing field";

      const auto result = match_json_nodes(child,
        expected.is_map() ? actual[child.key()] : actual[index],
        child_path);

      if (!result)
        return result;
      ++index;
    }
    return testing::AssertionSuccess();
  }

  if (expected.has_val() && (expected.val() != actual.val()
    || expected.is_val_quoted() != actual.is_val_quoted())) {
    return testing::AssertionFailure() << path << ": expected "
      << (expected.is_val_quoted() ? "quoted " : "unquoted ")
      << omni::ryml::detail::to_string_view(expected.val()) << ", got "
      << (actual.is_val_quoted() ? "quoted " : "unquoted ")
      << omni::ryml::detail::to_string_view(actual.val());
  }
  return testing::AssertionSuccess();
}

// Compare an already-parsed node against a readable JSON or YAML document.
inline testing::AssertionResult matches_node(std::string expected,
  ::ryml::ConstNodeRef actual) {
  const auto expected_tree = omni::ryml::core::parse(std::move(expected));
  if (!expected_tree)
    return testing::AssertionFailure() << "Invalid expected document: "
      << expected_tree.error();
  return match_json_nodes(expected_tree->crootref(), actual, "");
}

// Actual output may be JSON or YAML; the expected document is written as JSON.
inline testing::AssertionResult matches_json(std::string expected,
  std::string actual) {
  const auto actual_tree = omni::ryml::core::parse(std::move(actual));
  if (!actual_tree)
    return testing::AssertionFailure() << "Invalid output: "
      << actual_tree.error();
  return matches_node(std::move(expected), actual_tree->crootref());
}

} // namespace serialization_test
