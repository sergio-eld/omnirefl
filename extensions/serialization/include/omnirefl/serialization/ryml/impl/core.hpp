#pragma once

// FIXME: [low] [external] Freestanding: RapidYAML documents setjmp/longjmp
// for parse-error recovery without exceptions; freestanding implementations
// need not provide these functions.

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/traits.hpp>
#include <omnirefl/serialization/ryml/state.hpp>
#include <omnirefl/traits.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>
#include <c4/format.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <csetjmp>
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

    // A single hard error needs no warning-limit expansion. Transfer its
    // storage instead of allocating another issue vector at each ancestor.
    if (child.diagnostics.issues.size() == 1
      && !child.diagnostics.issues.front().warning
      && this->result.diagnostics.issues.empty() && !this->stopped()) {
      ++this->state.error_count;
      this->result.value.reset();
      this->result.diagnostics.issues = std::move(child.diagnostics.issues);
    } else {
      // report() includes the warning-limit error at the crossing warning.
      // Stopping there also skips a marker already included by the child.
      for (auto &entry : child.diagnostics.issues) {
        if (this->stopped())
          break;

        this->report(std::move(entry));
      }
    }

    if (!child.diagnostics.parse_error_message.empty()) {
      if (this->result.diagnostics.parse_error_message.empty())
        this->result.diagnostics.parse_error_message =
          std::move(child.diagnostics.parse_error_message);
      else {
        this->result.diagnostics.parse_error_message += '\n';
        this->result.diagnostics.parse_error_message +=
          child.diagnostics.parse_error_message;
      }
    }

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
      /*config=*/{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
        /*base=*/{},
#endif
        state.config.error_policy, {}},
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
  const auto scalar = node.val();
  const auto converted = omni::ryml::detail::from_chars<compat::remove_cv_t<T>>(
    scalar);

  if (!converted) {
    auto result = omni::ryml::detail::make_node_accumulator(
      std::move(context));
    result.report(core::make_issue(
      converted.error(),
      expected,
      node,
      /*warning=*/false));

    return {compat::nullopt, std::move(result.result.diagnostics)};
  }

  return {
    /*value=*/ *converted,
    /*diagnostics=*/{
      /*config=*/{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
        /*base=*/{},
#endif
        context.config.error_policy, {}},
      /*issues=*/{},
      /*parse_error_message=*/{},
    },
  };
}

} // namespace detail
} // namespace ryml
} // namespace omni

namespace omni {
namespace ryml {
namespace core {

inline compat::expected<::ryml::Tree, std::string> parse(std::string source,
  ::ryml::ParserOptions options) {
  // Static storage lets the parser callback use the signal without captures.
  static constexpr int parse_failed = 1;

  struct _error_t {
    std::jmp_buf jump;
    std::string message;
    ::ryml::Location location;
  } error{};

  // RapidYAML Quickstart, sample_error_handler(): non-returning error recovery.
  // https://rapidyaml.readthedocs.io/latest/doxygen/group__doc__quickstart.html
  // Both the parser and tree can report syntax errors during parsing.
  ::ryml::EventHandlerTree handler{
    ::ryml::Callbacks{}.set_user_data(&error).set_error_parse(
      [](c4::csubstr message, const ::ryml::ErrorDataParse &data, void *user) {
        const auto state = static_cast<_error_t *>(user);
        // ryml already formatted the message; copy it before its buffer
        // expires.
        state->message.assign(message.data(), message.size());
        state->location = data.ymlloc;

        // Exit the parser and resume setjmp with this nonzero error signal.
        std::longjmp(state->jump, parse_failed);
      }),
  };

  ::ryml::Tree tree{
    /*node_capacity=*/0,
    /*arena_capacity=*/0,
    /*callbacks=*/handler.callbacks(),
  };

  ::ryml::Parser parser{
    /*handler=*/&handler,
    /*options=*/options,
  };

  // Ad hoc recovery boundary: ryml reports syntax errors through a callback
  // that must not return, rather than a result. Keep setjmp in this lambda
  // so longjmp cannot make the modified outer tree/parser/error indeterminate
  // or skip their destructors.
  return compat::invoke( //<
    [&error, &tree, &parser, &source]()
      -> compat::expected<::ryml::Tree, std::string> {
      if (parse_failed == setjmp(error.jump))
        return compat::unexpected<std::string>{"line "
          + std::to_string(error.location.line + 1) + ", column "
          + std::to_string(error.location.col + 1) + ": " + error.message};

      ::ryml::parse_in_arena(&parser,
        c4::csubstr{source.data(), source.size()},
        &tree);

      // Do not let the returned tree retain the local recovery state.
      tree.callbacks(::ryml::Callbacks{});
      return std::move(tree);
    });
}

inline std::vector<issue::path_segment> node_path(::ryml::ConstNodeRef node) {
  if (node.invalid())
    return {};

  // TODO: [high] Benchmark counting path segments first, then allocating the
  // exact vector size and filling it in root-to-leaf order. Compare against
  // this single-pass collection and reversal before implementing.
  std::vector<issue::path_segment> path;
  for (auto current = node; current.has_parent(); current = current.parent()) {
    const auto parent = current.parent();
    if (parent.is_map())
      path.push_back({current.key(), compat::nullopt});
    else if (parent.is_seq())
      path.push_back({{}, parent.child_pos(current)});
  }

  std::reverse(path.begin(), path.end());
  return path;
}

inline issue make_issue(json::issue_code reason, json::value_kind expected,
  ::ryml::ConstNodeRef node, bool warning) {
  // C++11 cannot aggregate-initialize issue because it has member defaults.
  issue result{};
  result.reason = reason;
  result.expected = expected;
  result.actual = omni::ryml::detail::node_kind(node);
  result.warning = warning;
  result.path = node_path(node);
  if (!node.invalid())
    result.node_id = node.id();

  return result;
}

inline OMNI_CPP14_CONSTEXPR auto parse_boolean(compat::string_view text)
  -> compat::expected<bool, compat::string_view> {
  if (text == "true" || text == "True" || text == "TRUE")
    return true;

  if (text == "false" || text == "False" || text == "FALSE")
    return false;

  return compat::unexpected<compat::string_view>{text};
}

namespace detail {

inline bool is_boolean_node(::ryml::ConstNodeRef node) {
  return node.has_val() && !node.is_container() && !node.is_val_quoted()
    && (!node.has_val_tag()
      || ::ryml::TAG_BOOL == ::ryml::to_tag(node.val_tag()))
    && bool(parse_boolean({node.val().str, node.val().len}));
}

inline bool is_number_node(::ryml::ConstNodeRef node) {
  return node.has_val() && !node.is_container() && !node.is_val_quoted()
    && (!node.has_val_tag()
      || ::ryml::TAG_FLOAT == ::ryml::to_tag(node.val_tag())
      || ::ryml::TAG_INT == ::ryml::to_tag(node.val_tag()))
    && node.val().is_real();
}

} // namespace detail

inline auto expect_object_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue> {
  if (node.is_map())
    return node;

  return compat::unexpected<issue>{make_issue(
    json::issue_code::unexpected_kind, json::value_kind::mapping, node)};
}

inline auto expect_array_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue> {
  if (node.is_seq())
    return node;

  return compat::unexpected<issue>{make_issue(
    json::issue_code::unexpected_kind, json::value_kind::sequence, node)};
}

inline auto expect_scalar_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue> {
  if (node.has_val() && !node.is_container())
    return node;

  return compat::unexpected<issue>{make_issue(
    json::issue_code::unexpected_kind, json::value_kind::scalar, node)};
}

inline auto expect_value_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue> {
  if (node.has_val() || node.is_container())
    return node;

  return compat::unexpected<issue>{make_issue(
    json::issue_code::unexpected_kind, json::value_kind::unknown, node)};
}

inline auto expect_string_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue> {
  const bool matches = node.has_val() && !node.is_container()
    && (node.has_val_tag()
      ? ::ryml::TAG_STR == ::ryml::to_tag(node.val_tag())
      : node.is_val_quoted()
        || (!node.val_is_null() && !node.val().is_number()
          && !parse_boolean({node.val().str, node.val().len})));

  if (matches)
    return node;

  return compat::unexpected<issue>{make_issue(
    json::issue_code::unexpected_kind, json::value_kind::string, node)};
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<traits::is_string_like<T>,
    is_read_state<State>>::value))
auto map_node_to_string_like_t::operator()(State state,
  ::ryml::ConstNodeRef node,
  omni::type_t<T>) const
  -> with_diagnostics_t<T, diagnostics> {
  assert(node.has_val() && !node.is_container());
  const auto scalar = node.val();
  using storage_type = compat::remove_cvref_t<decltype(adapt::as_string(
    std::declval<const T &>()))>;

  static_assert(std::is_same<char, typename storage_type::value_type>::value,
    "Ryml string mapping currently supports only char text.");

  // TODO: [medium] Define UTF-8 conversion for non-char strings and handling
  // of invalid UTF-8 through the shared operation customization points.
  // See the disabled wide-string case in tests/string_mapping.cpp.

  using view_type = compat::basic_string_view<
    typename storage_type::value_type, typename storage_type::traits_type>;

  return {
    /*value=*/adapt::from_string(omni::type_t<T>{},
      view_type{scalar.str, scalar.len}),
    /*diagnostics=*/{
      /*config=*/{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
        /*base=*/{},
#endif
        state.config.error_policy, {}},
      /*issues=*/{},
      /*parse_error_message=*/{},
    },
  };
}

inline auto expect_boolean_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue> {
  if (detail::is_boolean_node(node))
    return node;

  return compat::unexpected<issue>{make_issue(
    json::issue_code::unexpected_kind, json::value_kind::boolean, node)};
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_same<bool, compat::remove_cv_t<T>>::value
    && !std::is_const<T>::value && is_read_state<State>::value))
auto map_node_to_boolean_t::operator()(State state,
  ::ryml::ConstNodeRef node,
  omni::type_t<T>) const
  -> with_diagnostics_t<T, diagnostics> {
  assert(node.has_val() && !node.is_container() && !node.is_val_quoted());
  const auto scalar = node.val();
  const auto result = parse_boolean({scalar.str, scalar.len});
  assert(result.has_value());
  return {
    /*value=*/ *result,
    /*diagnostics=*/{
      /*config=*/{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
        /*base=*/{},
#endif
        state.config.error_policy, {}},
      /*issues=*/{},
      /*parse_error_message=*/{},
    },
  };
}

inline auto expect_integer_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue> {
  const bool matches = node.has_val() && !node.is_container()
    && !node.is_val_quoted()
    && (!node.has_val_tag()
      || ::ryml::TAG_INT == ::ryml::to_tag(node.val_tag()))
    && node.val().is_integer();

  if (matches)
    return node;

  return compat::unexpected<issue>{make_issue(
    json::issue_code::unexpected_kind, json::value_kind::integer, node)};
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_integral<T>::value
    && !std::is_same<bool, compat::remove_cv_t<T>>::value
    && is_read_state<State>::value))
auto map_node_to_integral_t::operator()(State state,
  ::ryml::ConstNodeRef node,
  omni::type_t<T>) const
  -> with_diagnostics_t<T, diagnostics> {
  assert(core::expect_integer_node(node));
  const auto expected = std::is_unsigned<T>::value
    ? json::value_kind::unsigned_integer : json::value_kind::integer;

  return omni::ryml::detail::read_scalar(std::move(state),
    node,
    omni::type_t<T>{},
    expected);
}

inline auto expect_number_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue> {
  if (detail::is_number_node(node))
    return node;

  return compat::unexpected<issue>{make_issue(
    json::issue_code::unexpected_kind, json::value_kind::number, node)};
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_floating_point<T>::value && is_read_state<State>::value))
auto map_node_to_floating_point_t::operator()(State state,
  ::ryml::ConstNodeRef node,
  omni::type_t<T>) const
  -> with_diagnostics_t<T, diagnostics> {
  assert(core::expect_number_node(node));
  return omni::ryml::detail::read_scalar(std::move(state),
    node,
    omni::type_t<T>{},
    json::value_kind::number);
}

inline auto expect_arithmetic_node(::ryml::ConstNodeRef node)
  -> compat::expected<::ryml::ConstNodeRef, issue> {
  if (detail::is_boolean_node(node) || detail::is_number_node(node))
    return node;

  return compat::unexpected<issue>{make_issue(
    json::issue_code::unexpected_kind, json::value_kind::scalar, node)};
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_arithmetic<T>::value && is_read_state<State>::value
    && (!std::is_const<T>::value
      || !std::is_same<bool, compat::remove_cv_t<T>>::value)))
auto map_node_to_arithmetic_t::operator()(State state,
  ::ryml::ConstNodeRef node,
  omni::type_t<T>) const
  -> with_diagnostics_t<T, diagnostics> {
  assert(node.has_val() && !node.is_container());
  constexpr auto read = typename traits::select<
    traits::case_<std::is_same<bool, compat::remove_cv_t<T>>::value,
      core::map_node_to_boolean_t>,
    traits::case_<std::is_integral<T>::value, core::map_node_to_integral_t>,
    core::map_node_to_floating_point_t>::type{};

  return read(std::move(state), node, omni::type_t<T>{});
}

} // namespace core

namespace detail {

inline diagnostics depth_error(::ryml::ConstNodeRef node) {
  return {
    /*config=*/{},
    /*issues=*/{core::make_issue(json::issue_code::depth_limit_exceeded,
      json::value_kind::unknown, node)},
    /*parse_error_message=*/{},
  };
}

} // namespace detail

inline std::string render_diagnostics_t::operator()(const ::ryml::Tree &tree,
  const diagnostics &d) const {
  std::string message;
  // Parse-error text is aggregated for the report, not stored per issue.
  bool parse_errors_rendered = false;
  for (const auto &entry : d.issues) {
    const bool parse_error = json::issue_code::parse_error == entry.reason;
    if (parse_error && parse_errors_rendered)
      continue;

    if (!message.empty())
      message += '\n';

    if (parse_error) {
      message += d.parse_error_message;
      parse_errors_rendered = true;
      continue;
    }

    const auto path = detail::render_path(entry.path);

    message += path.empty() ? std::string{"<root>"} : path;
    message += ": ";
    if (entry.warning)
      message += "warning: ";

    switch (entry.reason) {
    case json::issue_code::depth_limit_exceeded:
      message += "tree depth limit exceeded";
      break;

    case json::issue_code::warning_limit_exceeded:
      message += "warning limit exceeded";
      break;

    case json::issue_code::missing_field:
      message += "missing field";
      break;

    case json::issue_code::unknown_field:
      message += "unknown field";
      break;

    case json::issue_code::duplicate_field:
      message += "duplicate field";
      break;

    case json::issue_code::duplicate_element:
      message += "duplicate element";
      break;

    case json::issue_code::unexpected_kind: {
      const auto expected = detail::kind_name(entry.expected);
      const auto actual = detail::kind_name(entry.actual);
      message += "expected ";
      message.append(expected.data(), expected.size());
      message += ", found ";
      message.append(actual.data(), actual.size());
      break;
    }

    default: {
      const auto value = detail::to_string_view(
        entry.node_id != ::ryml::NONE && tree.has_val(entry.node_id)
          ? tree.val(entry.node_id) : c4::csubstr{});

      message += '"';
      message.append(value.data(), value.size());
      message += "\" ";
      if (json::issue_code::out_of_range == entry.reason)
        message += "is out of range";
      else {
        message += "is not ";
        message += json::value_kind::integer == entry.expected
            || json::value_kind::unsigned_integer == entry.expected
          ? "an " : "a ";

        const auto expected = detail::kind_name(entry.expected);
        message.append(expected.data(), expected.size());
      }

      break;
    }
    }
  }

  return message;
}

} // namespace ryml
} // namespace omni
