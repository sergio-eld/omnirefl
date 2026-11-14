#pragma once

#include <omnirefl/functional.hpp>

#include <c4/format.hpp>
#include <c4/std/string.hpp>

#include <algorithm>
#include <cassert>
// FIXME: [low] Freestanding: parse-error recovery requires setjmp/longjmp,
// which are not guaranteed by a freestanding implementation.
#include <csetjmp>
#include <iterator>
#include <limits>
#include <type_traits>

// TODO: Provide an omni::compat::string_view drop-in for the C++11 baseline.
#if 201703L <= OMNI_CPLUSPLUS && defined(__has_include)
#  if __has_include(<string_view>)
#    include <string_view>
#  endif
#endif

namespace omni {
namespace ryml {

inline std::vector<issue::path_segment> mapping_path::materialize() const {
  std::vector<issue::path_segment> segments(this->depth);
  auto path = this;
  for (std::size_t remaining = this->depth; remaining; --remaining) {
    segments[remaining - 1] = path->segment;
    path = path->parent;
  }

  return segments;
}

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

inline std::string render_path(const std::vector<issue::path_segment> &segments) {
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
  mapping_state<Config> state;
  with_diagnostics<compat::monostate, diagnostics> result;

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

  void append(with_diagnostics<compat::monostate, diagnostics> child) {
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

  auto finish() && -> with_diagnostics<compat::monostate, diagnostics> {
    return std::move(this->result);
  }
};

template <typename Config>
node_accumulator<Config> make_node_accumulator(mapping_state<Config> state) {
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

  if (!node.invalid()) {
    result.node_id = node.id();
  }

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

namespace core {

// Core algorithm

template <typename T, typename Config>
void write_node(const T &value,
  ::ryml::NodeRef node,
  mapping_state<Config> context) {
  cp::from_type<T>{}(value, node, context);
}

template <typename T, typename Config>
auto read_node(::ryml::ConstNodeRef node,
  T *into,
  mapping_state<Config> context)
  -> with_diagnostics<compat::monostate, diagnostics> {
  auto result = omni::ryml::detail::make_node_accumulator(context);
  if (context.limit_exceeded()) {
    result.result.value.reset();
  } else {
    result.append(omni::ryml::to_type(omni::type_t<T>{})(node, into, context));
  }
  return std::move(result).finish();
}

namespace detail {

template <bool BorrowStrings, typename T>
struct cpp11_lambda_write {
  const T &value;

  template <typename _M, omni::reflected_entity Entity>
  ::ryml::Tree operator()(omni::meta_t<_M, Entity>) const {
    ::ryml::Tree tree;
    const config<> options{};
    const mapping_state<> context{
      /*config=*/options,
      /*path=*/{},
      /*error_count=*/0,
      /*warning_count=*/0,
      /*borrow_strings=*/BorrowStrings,
    };

    core::write_node(this->value, tree.rootref(), context);
    return tree;
  }
};

template <typename T, typename Config>
struct cpp11_lambda_read {
  const ::ryml::ConstNodeRef &node;
  T &into;
  mapping_state<Config> context;

  template <typename _M, omni::reflected_entity Entity>
  auto operator()(omni::meta_t<_M, Entity>) const
    -> with_diagnostics<compat::monostate, diagnostics> {
    return core::read_node(this->node,
      &this->into,
      this->context);
  }
};

} // namespace detail

inline compat::expected<::ryml::Tree, std::string> parse(std::string source,
  ::ryml::ParserOptions options) {
  // Static storage lets the parser callback use the signal without captures.
  static constexpr int parse_failed = 1;

  struct _error_t {
    std::jmp_buf jump;
    std::string message;
    ::ryml::Location location;
  } error{};

  // ryml documents jump recovery for its non-returning error callbacks.
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
  return compat::invoke( //
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

namespace detail {

OMNI_CONSTRAINED_TEMPLATE(
  (typename T,
    typename Config,
    typename Discovery = compat::remove_cvref_t<T>),

  (cp::reflect_as_root<Discovery>::value))
auto map_tree_dispatch(::ryml::ConstNodeRef node,
  T *into,
  mapping_state<Config> context)
  -> with_diagnostics<compat::monostate, diagnostics> {
  return omni::reflected_call(
    cpp11_lambda_read<T, Config>{
      /*node=*/node,
      /*into=*/ *into,
      /*context=*/context,
    },
    omni::type_t<Discovery>{});
}

OMNI_CONSTRAINED_TEMPLATE(
  (typename T,
    typename Config,
    typename Discovery = compat::remove_cvref_t<T>),

  (!cp::reflect_as_root<Discovery>::value &&
    !compat::disjunction<omni::ryml::detail::is_mapped_range<Discovery>,
    traits::is_optional<Discovery>>::value))
auto map_tree_dispatch(::ryml::ConstNodeRef node,
  T *into,
  mapping_state<Config> context)
  -> with_diagnostics<compat::monostate, diagnostics> {
  return read_node(node, into, context);
}

OMNI_CONSTRAINED_TEMPLATE(
  (typename T,
    typename Config,
    typename Discovery = compat::remove_cvref_t<T>),

  (!cp::reflect_as_root<Discovery>::value &&
    compat::disjunction<omni::ryml::detail::is_mapped_range<Discovery>,
    traits::is_optional<Discovery>>::value))
auto map_tree_dispatch(::ryml::ConstNodeRef node,
  T *into,
  mapping_state<Config> context)
  -> with_diagnostics<compat::monostate, diagnostics> {
  return map_tree_dispatch<T, Config,
    typename omni::ryml::detail::discovered_range_type<Discovery>::type>(
    node, into, context);
}

} // namespace detail

template <typename T, typename TransformDiagnostics>
with_diagnostics<T> map_tree(const config<TransformDiagnostics> &options,
  omni::type_t<T>, ::ryml::ConstNodeRef node) {
  auto value = compat::make_optional<T>();
  const mapping_state<config<TransformDiagnostics>> context{
    /*config=*/options,
    /*path=*/{},
    /*error_count=*/0,
    /*warning_count=*/0,
    /*borrow_strings=*/false,
  };

  auto mapped = detail::map_tree_dispatch(node, &*value, context);
  return {
    /*value=*/mapped.value ? std::move(value) : compat::nullopt,
    /*diagnostics=*/std::move(mapped.diagnostics),
  };
}

template <typename T, typename TransformDiagnostics>
with_diagnostics<T> map_tree(const config<TransformDiagnostics> &options,
  omni::type_t<T> type, const ::ryml::Tree &tree) {
  return map_tree(options, type, tree.crootref());
}

#if defined(__cpp_concepts) && 201907L <= __cpp_concepts
template <typename Config, typename Record>
  requires (traits::is_specialization_of<config, Config>())
#else
template <typename Config, typename Record,
  compat::enable_if_t<traits::is_specialization_of<config, Config>(), int>>
#endif
auto deserialize(Config &&options, omni::type_t<Record> record_type, std::string source)
  -> with_diagnostics<Record, diagnostics_result_t<Config>> {
  auto parsed = omni::ryml::core::parse(std::move(source));
  if (!parsed) {
    const ::ryml::Tree tree{/*node_capacity=*/0, /*arena_capacity=*/0};
    diagnostics report{
      /*config=*/{options.error_policy, {}},
      /*issues=*/{
        omni::ryml::detail::make_issue(json::issue_code::parse_error,
          json::value_kind::unknown, ::ryml::ConstNodeRef{}, /*path=*/{}),
      },
      /*parse_error_message=*/std::move(parsed.error()),
    };

    return {
      /*value=*/compat::nullopt,
      /*diagnostics=*/compat::invoke(
        std::forward<Config>(options).transform_diagnostics, tree,
        std::move(report)),
    };
  }

  auto mapped = map_tree(options, record_type, parsed->crootref());
  return {
    /*value=*/std::move(mapped.value),
    /*diagnostics=*/compat::invoke(
      std::forward<Config>(options).transform_diagnostics,
      static_cast<const ::ryml::Tree &>(*parsed),
      std::move(mapped.diagnostics)),
  };
}

namespace detail {

OMNI_CONSTRAINED_TEMPLATE(
  (typename T, typename Discovery = compat::remove_cvref_t<T>),
  (cp::reflect_as_root<Discovery>::value))
::ryml::Tree as_owning_tree_dispatch(const T &value) {
  return omni::reflected_call(
    cpp11_lambda_write</*BorrowStrings=*/false, T>{/*value=*/value},
    omni::type_t<Discovery>{});
}

OMNI_CONSTRAINED_TEMPLATE(
  (typename T, typename Discovery = compat::remove_cvref_t<T>),
  (!cp::reflect_as_root<Discovery>::value &&
    !compat::disjunction<omni::ryml::detail::is_mapped_range<Discovery>,
    traits::is_optional<Discovery>>::value))
::ryml::Tree as_owning_tree_dispatch(const T &value) {
  ::ryml::Tree tree;
  const config<> options{};
  const mapping_state<> context{
    /*config=*/options,
    /*path=*/{},
    /*error_count=*/0,
    /*warning_count=*/0,
    /*borrow_strings=*/false,
  };

  write_node(value, tree.rootref(), context);
  return tree;
}

OMNI_CONSTRAINED_TEMPLATE(
  (typename T, typename Discovery = compat::remove_cvref_t<T>),
  (!cp::reflect_as_root<Discovery>::value &&
    compat::disjunction<omni::ryml::detail::is_mapped_range<Discovery>,
    traits::is_optional<Discovery>>::value))
::ryml::Tree as_owning_tree_dispatch(const T &value) {
  return as_owning_tree_dispatch<T,
    typename omni::ryml::detail::discovered_range_type<Discovery>::type>(value);
}

OMNI_CONSTRAINED_TEMPLATE(
  (typename T, typename Discovery = compat::remove_cvref_t<T>),
  (cp::reflect_as_root<Discovery>::value))
::ryml::Tree as_ref_tree_dispatch(T &value) {
  return omni::reflected_call(
    cpp11_lambda_write</*BorrowStrings=*/true, T>{/*value=*/value},
    omni::type_t<Discovery>{});
}

OMNI_CONSTRAINED_TEMPLATE(
  (typename T, typename Discovery = compat::remove_cvref_t<T>),
  (!cp::reflect_as_root<Discovery>::value &&
    !compat::disjunction<omni::ryml::detail::is_mapped_range<Discovery>,
    traits::is_optional<Discovery>>::value))
::ryml::Tree as_ref_tree_dispatch(T &value) {
  ::ryml::Tree tree;
  const config<> options{};
  const mapping_state<> context{
    /*config=*/options,
    /*path=*/{},
    /*error_count=*/0,
    /*warning_count=*/0,
    /*borrow_strings=*/true,
  };

  write_node(value, tree.rootref(), context);
  return tree;
}

OMNI_CONSTRAINED_TEMPLATE(
  (typename T, typename Discovery = compat::remove_cvref_t<T>),
  (!cp::reflect_as_root<Discovery>::value &&
    compat::disjunction<omni::ryml::detail::is_mapped_range<Discovery>,
    traits::is_optional<Discovery>>::value))
::ryml::Tree as_ref_tree_dispatch(T &value) {
  return as_ref_tree_dispatch<T,
    typename omni::ryml::detail::discovered_range_type<Discovery>::type>(value);
}

} // namespace detail

template <typename T>
::ryml::Tree as_owning_tree(const T &value) {
  return detail::as_owning_tree_dispatch(value);
}

template <typename T>
::ryml::Tree as_ref_tree(T &value) {
  return detail::as_ref_tree_dispatch(value);
}

} // namespace core

inline std::string render_diagnostics_t::operator()(const ::ryml::Tree &tree,
  const diagnostics &d) const {
  std::string message;
  for (const auto &entry : d.issues) {
    if (!message.empty())
      message += '\n';

    if (json::issue_code::parse_error == entry.reason) {
      message += d.parse_error_message;
      continue;
    }

    const auto path = detail::render_path(entry.path);

    message += path.empty() ? std::string{"<root>"} : path;
    message += ": ";
    if (entry.warning)
      message += "warning: ";

    switch (entry.reason) {
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
      if (json::issue_code::out_of_range == entry.reason) {
        message += "could not be converted to the destination scalar";
      } else {
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

template <typename Value>
std::string serialize_t::operator()(const Value &value) const {
  const auto tree = core::as_ref_tree(value);

  // Reserve an initial estimate without a sizing traversal. Borrowed strings,
  // escaping, or deeper YAML indentation may require an emitter resize/retry.
  std::string text;
  text.reserve(
    tree.arena_size() + (::ryml::EMIT_JSON == format ? 8 : 16) * tree.size());

  if (::ryml::EMIT_JSON != format) {
    ::ryml::emitrs_yaml(tree, &text);
    return text;
  }

  ::ryml::emitrs_json(tree, &text);

  // JSON strings must escape ASCII control characters (0x00-0x1F).
  // This output is compact, so raw tabs/newlines cannot be formatting
  // whitespace.
  // FIXME: [external] RapidYAML leaves some controls unescaped, even in 0.16.0.
  // Remove this pass once upstream escapes the full 0x00-0x1F range.
  // https://github.com/biojppm/rapidyaml/blob/master/src/c4/yml/emitter.def.hpp
  const auto is_control_byte = [](unsigned char c) { return 0x20 > c; };
  const auto controls =
    std::count_if(text.begin(), text.end(), is_control_byte);

  if (0 == controls)
    return text;

  // Subtract the escape's terminator and the input byte being replaced.
  const auto extra = (sizeof(R"(\u0000)") - 2) * controls;
  text.resize(text.size() + extra);

  // Write backwards so expansion cannot overwrite unread input.
  auto output = text.rbegin();
  for (auto input = text.rbegin() + extra; input != text.rend(); ++input) {
    const unsigned char c = *input;
    if (!is_control_byte(c)) {
      *output++ = c;
      continue;
    }

    const char escaped[]{
      '\\',
      'u',
      '0',
      '0',
      "0123456789abcdef"[c >> 4],
      "0123456789abcdef"[c & 0x0f],
    };

    output = std::reverse_copy(std::begin(escaped), std::end(escaped), output);
  }

  return text;
}

} // namespace ryml
} // namespace omni
