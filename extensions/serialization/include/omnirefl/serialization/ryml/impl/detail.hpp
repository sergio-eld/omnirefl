#pragma once

#include <omnirefl/serialization/ryml/detail.hpp>

#include <c4/format.hpp>
#include <c4/std/string.hpp>

#include <cassert>
#include <limits>
#include <utility>

namespace omni {
namespace ryml {

namespace detail {

constexpr compat::string_view to_string_view(c4::csubstr s) {
  // Keep empty views safe to append through C++11 string's pointer/size API.
  return {s.len ? s.str : "", s.len};
}

inline json::value_kind node_kind(::ryml::ConstNodeRef n) {
  if (n.invalid())
    return json::value_kind::unknown;

  if (n.is_map())
    return json::value_kind::mapping;

  if (n.is_seq())
    return json::value_kind::sequence;

  if (n.has_val())
    return n.val_is_null() ? json::value_kind::null : json::value_kind::scalar;

  return json::value_kind::unknown;
}

inline std::string render_path(
  const std::vector<issue::path_segment> &segments) {
  std::string path;
  for (const auto &segment : segments) {
    path += '/';
    if (segment.index) {
      path += std::to_string(*segment.index);
      continue;
    }

    for (const char ch : segment.field) {
      if ('~' == ch)
        path += "~0";
      else if ('/' == ch)
        path += "~1";
      else
        path += ch;
    }
  }

  return path;
}

OMNI_CPP14_CONSTEXPR inline compat::string_view kind_name(json::value_kind k) {
  switch (k) {
  case json::value_kind::mapping:
    return "object";
  case json::value_kind::sequence:
    return "array";
  case json::value_kind::string:
    return "string";
  case json::value_kind::boolean:
    return "boolean";
  case json::value_kind::integer:
    return "integer";
  case json::value_kind::unsigned_integer:
    return "unsigned integer";
  case json::value_kind::number:
    return "real number";
  case json::value_kind::null:
    return "null";
  default:
    return "scalar";
  }
}

// Local traversal output owns the accumulated diagnostics and limit counters.
// Child states snapshot progress; diagnostics are returned separately.
template <typename Config>
struct node_accumulator {
  read_state_t<Config> state;
  with_diagnostics_t<compat::monostate, diagnostics> result;

  bool stopped() const { return this->state.limit_exceeded(); }

  void report(issue entry) {
    this->state.error_count += !entry.warning;
    this->state.warning_count += entry.warning;
    if (!entry.warning)
      this->result.value.reset();

    this->result.diagnostics.issues.push_back(std::move(entry));
    if (this->result.diagnostics.issues.back().warning
      && this->state.config.error_policy.warning_limit
      && this->state.warning_count
        == std::size_t{1} + *this->state.config.error_policy.warning_limit) {
      auto exceeded = this->result.diagnostics.issues.back();
      exceeded.reason = json::issue_code::warning_limit_exceeded;
      exceeded.warning = false;
      this->result.diagnostics.issues.push_back(std::move(exceeded));
      ++this->state.error_count;
      this->result.value.reset();
    }
  }

  void append(with_diagnostics_t<compat::monostate, diagnostics> child) {
    if (!child.value)
      this->result.value.reset();

    // A child may already have emitted the limit error. Preserve that report
    // without generating the same warning-limit error again at each parent.
    const bool has_limit_error = std::any_of(child.diagnostics.issues.begin(),
      child.diagnostics.issues.end(),
      [](const issue &entry) {
        return entry.reason == json::issue_code::warning_limit_exceeded;
      });

    for (auto &entry : child.diagnostics.issues) {
      if (has_limit_error) {
        this->state.error_count += !entry.warning;
        this->state.warning_count += entry.warning;
        if (!entry.warning)
          this->result.value.reset();

        this->result.diagnostics.issues.push_back(std::move(entry));
      } else {
        this->report(std::move(entry));
      }
    }
    if (this->stopped())
      this->result.value.reset();
  }

  auto finish() && -> with_diagnostics_t<compat::monostate, diagnostics> {
    return std::move(this->result);
  }
};

template <typename Config>
node_accumulator<Config> make_node_accumulator(read_state_t<Config> state) {
  return {
    /*state=*/state,
    /*result=*/{
      /*value=*/compat::monostate{},
      /*diagnostics=*/{
        /*config=*/{state.config.error_policy, {}},
        /*issues=*/{},
        /*parse_error_message=*/{},
      },
    },
  };
}

inline issue make_issue(json::issue_code reason,
  json::value_kind expected,
  ::ryml::ConstNodeRef node,
  mapping_path path,
  bool warning = false) {
  // C++11 cannot aggregate-initialize issue because it has member defaults.
  issue result{};
  result.reason = reason;
  result.path = path.materialize();
  result.expected = expected;
  result.actual = node_kind(node);
  result.warning = warning;

  if (!node.invalid())
    result.node_id = node.id();

  return result;
}

// Format validation precedes conversion; failure here means out of range.
OMNI_CONSTRAINED_TEMPLATE((typename T), (std::is_integral<T>::value))
compat::expected<T, json::issue_code> from_chars(c4::csubstr scalar) {
  T value{};
  // C4 accepts a pointer to either the checked wrapper or the original value.
  compat::conditional_t<std::is_same<bool, T>::value,
    T &,
    c4::fmt::overflow_checked_<T>> target{value};

  if (!c4::from_chars(scalar, &target))
    return compat::unexpected<json::issue_code>{json::issue_code::out_of_range};

  return value;
}

OMNI_CONSTRAINED_TEMPLATE((typename T), (!std::is_integral<T>::value))
compat::expected<T, json::issue_code> from_chars(c4::csubstr scalar) {
  T value{};
  if (!c4::from_chars(scalar, &value))
    return compat::unexpected<json::issue_code>{json::issue_code::out_of_range};

  return value;
}

struct write_integer {
  template <typename Value>
  void operator()(const Value &value, ::ryml::NodeRef *node) const {
    assert(node);
    node->set_serialized(value);
  }
};

struct write_boolean {
  void operator()(bool value, ::ryml::NodeRef *node) const {
    assert(node);
    // c4's plain bool conversion emits 0/1, rather than document booleans.
    node->set_serialized(c4::fmt::boolalpha(value));
  }
};

struct write_real {
  template <typename Value>
  void operator()(const Value &value, ::ryml::NodeRef *node) const {
    assert(node);
    // Preserve enough significant digits to recover the original value.
    node->set_serialized(c4::fmt::real(value,
      std::numeric_limits<Value>::max_digits10,
      c4::FTOA_FLEX));
  }
};

} // namespace detail

} // namespace ryml
} // namespace omni
