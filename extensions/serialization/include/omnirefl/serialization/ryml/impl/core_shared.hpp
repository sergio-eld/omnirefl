#pragma once

/** Conversion and diagnostic helpers independent of type mapping. */

#include <omnirefl/serialization/ryml/state.hpp>
#include <omnirefl/extensions/traits.hpp>

#include <c4/format.hpp>

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// c4 views use the same operation customization points as user strings.
template <>
struct omni::traits::cp::as_string<c4::csubstr> {
  constexpr omni::compat::string_view operator()(c4::csubstr value) const {
    return {value.str, value.len};
  }
};

template <>
struct omni::traits::cp::from_string<c4::csubstr> {
  c4::csubstr operator()(omni::compat::string_view text) const {
    return {text.data(), text.size()};
  }
};

namespace omni {
namespace ryml {
namespace core {

// Scalar conversion reports issues before core.hpp supplies the definition.
inline issue make_issue(json::issue_code reason, json::value_kind expected,
  ::ryml::ConstNodeRef node, bool warning);

} // namespace core

namespace detail {

// Strings remain scalar mappings even though they also model input ranges.
template <typename T>
struct is_mapped_range:
    compat::conjunction<compat::is_input_range<T>,
      compat::negation<traits::is_string_like<compat::remove_cvref_t<T>>>> {};

// C++11 overload ranking for callback signatures and field destinations.
// REFACTORME: [medium] Simplify ranked dispatch for callback signatures and
// field destinations while preserving overload precedence.
template <int Rank>
struct node_call_priority: node_call_priority<Rank - 1> {};

template <>
struct node_call_priority<0> {};

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
template <typename State>
struct node_accumulator {
  State state;
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

    // report() includes the warning-limit error at the crossing warning.
    // Stopping there also skips a marker already included by the child.
    for (auto &entry : child.diagnostics.issues) {
      if (this->stopped())
        break;

      this->report(std::move(entry));
    }

    if (!child.diagnostics.parse_error_message.empty())
      this->result.diagnostics.parse_error_message = append_diagnostics_t{}(
        std::move(this->result.diagnostics.parse_error_message),
        std::move(child.diagnostics.parse_error_message));

    if (this->stopped())
      this->result.value.reset();
  }

  auto finish() && -> with_diagnostics_t<compat::monostate, diagnostics> {
    return std::move(this->result);
  }
};

OMNI_CONSTRAINED_TEMPLATE((typename State), (is_read_state<State>::value))
node_accumulator<State> make_node_accumulator(State state) {
  // Capture diagnostic policies before consuming the owned state.
  with_diagnostics_t<compat::monostate> result{
    /*value=*/compat::monostate{},
    /*diagnostics=*/{
      /*config=*/{state.config.error_policy, {}},
      /*issues=*/{},
      /*parse_error_message=*/{},
    },
  };

  return {/*state=*/std::move(state), /*result=*/std::move(result)};
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

// Shared checked conversion; node format is validated before this call.
template <typename T, typename State>
auto read_scalar(State context,
  ::ryml::ConstNodeRef node,
  omni::type_t<T>,
  json::value_kind expected)
  -> with_diagnostics_t<T, diagnostics> {
  auto result = omni::ryml::detail::make_node_accumulator(
    std::move(context));
  const auto scalar = node.val();
  const auto converted = omni::ryml::detail::from_chars<compat::remove_cv_t<T>>(
    scalar);

  if (!converted) {
    result.report(core::make_issue(
      converted.error(),
      expected,
      node,
      /*warning=*/false));

    return {compat::nullopt, std::move(result.result.diagnostics)};
  }

  return {
    /*value=*/ *converted,
    /*diagnostics=*/std::move(result.result.diagnostics),
  };
}

} // namespace detail
} // namespace ryml
} // namespace omni
