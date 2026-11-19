#pragma once

#include <c4/std/string.hpp>

#include <algorithm>
#include <cassert>
#include <iterator>
#include <type_traits>

namespace omni {
namespace ryml {
namespace core {

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (is_read_state<State>::value))
auto read_node(::ryml::ConstNodeRef node, T *into, State context)
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  auto result = omni::ryml::detail::make_node_accumulator(context);
  if (context.limit_exceeded())
    result.result.value.reset();
  else {
    const auto mapping =
      omni::ryml::reflected_scope::mapping_to_type(omni::type_t<T>{});

    result.append(omni::ryml::detail::read_into(
      mapping, node, into, std::move(context), /*priority=*/0));
  }

  return std::move(result).finish();
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (is_write_state<State>::value))
void write_node(const T &value, ::ryml::NodeRef node, State context) {
  omni::ryml::reflected_scope::mapping_from_type(omni::type_t<T>{})(
    value, node, std::move(context));
}

namespace detail {

// A callable defers registration lookup until the non-reflected branch is
// selected, including in C++11. Its explicit default needs no metadata, unlike
// reflected_scope::mapping_to_type's built-in classification.
template <typename Default>
struct map_registered_or {
  template <typename T, typename State>
  auto operator()(::ryml::ConstNodeRef node, omni::type_t<T> type,
    State context) const
    -> with_diagnostics_t<T, diagnostics> {
    const cp::to_type_t<T> mapping_t{};
    const auto mapping_var =
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
      cp::to_type<T>;
#else
      traits::cp::unavailable{};
#endif

    static_assert(traits::cp::unavailable{} == mapping_t
        || traits::cp::unavailable{} == mapping_var,
      "Specialize either cp::to_type_t or cp::to_type, not both");

    const auto mapping = omni::ryml::detail::first_available(
      mapping_t, mapping_var, Default{});

    static_assert(std::is_trivially_copyable<decltype(mapping)>::value
        && std::is_copy_constructible<decltype(mapping)>::value,
      "Node mapping callables must be trivially copyable "
      "and copy-constructible");

    return omni::ryml::detail::map_node_to_value(
      mapping, node, type, std::move(context));
  }

  template <typename T, typename State>
  auto operator()(::ryml::ConstNodeRef node, T *into, State context) const
    -> with_diagnostics_t<compat::monostate, diagnostics> {
    const cp::to_type_t<T> mapping_t{};
    const auto mapping_var =
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
      cp::to_type<T>;
#else
      traits::cp::unavailable{};
#endif

    static_assert(traits::cp::unavailable{} == mapping_t
        || traits::cp::unavailable{} == mapping_var,
      "Specialize either cp::to_type_t or cp::to_type, not both");

    const auto mapping = omni::ryml::detail::first_available(
      mapping_t, mapping_var, Default{});

    static_assert(std::is_trivially_copyable<decltype(mapping)>::value
        && std::is_copy_constructible<decltype(mapping)>::value,
      "Node mapping callables must be trivially copyable "
      "and copy-constructible");

    auto result = omni::ryml::detail::make_node_accumulator(context);
    if (context.limit_exceeded())
      result.result.value.reset();
    else
      result.append(omni::ryml::detail::read_into(
        mapping, node, into, std::move(context), /*priority=*/0));

    return std::move(result).finish();
  }
};

// reflected_call needs a metadata-bearing type, even for an empty range.
// Only discovery unwraps wrappers; the mapped destination stays T.
template <typename T, typename = void>
struct reflection_target { using type = T; };

template <typename T>
struct reflection_target<T,
  compat::enable_if_t<!cp::reflect_as_root<T>::value
    && compat::disjunction<omni::ryml::detail::is_mapped_range<T>,
      traits::is_optional<T>>::value>>:
    reflection_target<
      typename omni::ryml::detail::discovered_range_type<T>::type> {};

} // namespace detail

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_arithmetic<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_fundamental_t::operator()(::ryml::ConstNodeRef node,
  omni::type_t<T> type, State context) const
  -> with_diagnostics_t<T, diagnostics> {
  return detail::map_registered_or<default_to_arithmetic_t>{}(
    node, type, std::move(context));
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_arithmetic<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_fundamental_t::operator()(::ryml::ConstNodeRef node, T *into,
  State context) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  assert(into);
  return detail::map_registered_or<default_to_arithmetic_t>{}(
    node, into, std::move(context));
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (traits::is_string_like<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_string_like_t::operator()(::ryml::ConstNodeRef node,
  omni::type_t<T> type, State context) const
  -> with_diagnostics_t<T, diagnostics> {
  return detail::map_registered_or<default_to_string_like_t>{}(
    node, type, std::move(context));
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (traits::is_string_like<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_string_like_t::operator()(::ryml::ConstNodeRef node, T *into,
  State context) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  assert(into);
  return detail::map_registered_or<default_to_string_like_t>{}(
    node, into, std::move(context));
}

namespace detail {

// A separate visitor keeps value construction inside reflected scope without
// creating or assigning a destination before selecting its mapping.
template <typename T, typename State>
struct cpp11_lambda_read_value {
  const ::ryml::ConstNodeRef &node;
  State context;

  template <typename _M, omni::reflected_entity Entity>
  auto operator()(omni::meta_t<_M, Entity>) const
    -> with_diagnostics_t<T> {
    const auto mapping =
      omni::ryml::reflected_scope::mapping_to_type(omni::type_t<T>{});

    return omni::ryml::detail::map_node_to_value(
      mapping, this->node, omni::type_t<T>{}, this->context);
  }
};

template <typename T, typename State>
struct cpp11_lambda_read {
  const ::ryml::ConstNodeRef &node;
  T &into;
  State context;

  template <typename _M, omni::reflected_entity Entity>
  auto operator()(omni::meta_t<_M, Entity>) const
    -> with_diagnostics_t<compat::monostate, diagnostics> {
    return core::read_node(this->node,
      &this->into,
      this->context);
  }
};

} // namespace detail

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_object<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_reflected_t::operator()(::ryml::ConstNodeRef node,
  omni::type_t<T>, State context) const
  -> with_diagnostics_t<T, diagnostics> {
  using target = typename detail::reflection_target<T>::type;
  return omni::reflected_call(
    detail::cpp11_lambda_read_value<T, State>{
      /*node=*/node,
      /*context=*/std::move(context),
    },
    omni::type_t<target>{});
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_object<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_reflected_t::operator()(::ryml::ConstNodeRef node, T *into,
  State context) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  assert(into);
  using target = typename detail::reflection_target<T>::type;
  return omni::reflected_call(
    detail::cpp11_lambda_read<T, State>{
      /*node=*/node,
      /*into=*/ *into,
      /*context=*/std::move(context),
    },
    omni::type_t<target>{});
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (omni::ryml::detail::is_mapped_range<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_sequence_t::operator()(::ryml::ConstNodeRef node,
  omni::type_t<T> type, State context) const
  -> with_diagnostics_t<T, diagnostics> {
  using target = typename detail::reflection_target<T>::type;
  constexpr auto map = typename traits::select<
    traits::case_<cp::reflect_as_root<target>::value, map_to_reflected_t>,
    detail::map_registered_or<
      decltype(default_to_range(omni::type_t<T>{}))>>::type{};

  return map(node, type, std::move(context));
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (omni::ryml::detail::is_mapped_range<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_sequence_t::operator()(::ryml::ConstNodeRef node, T *into,
  State context) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  assert(into);
  using target = typename detail::reflection_target<T>::type;
  constexpr auto map = typename traits::select<
    traits::case_<cp::reflect_as_root<target>::value, map_to_reflected_t>,
    detail::map_registered_or<
      decltype(default_to_range(omni::type_t<T>{}))>>::type{};

  return map(node, into, std::move(context));
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (traits::is_optional<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_optional_t::operator()(::ryml::ConstNodeRef node,
  omni::type_t<T> type, State context) const
  -> with_diagnostics_t<T, diagnostics> {
  using target = typename detail::reflection_target<T>::type;
  constexpr auto map = typename traits::select<
    traits::case_<cp::reflect_as_root<target>::value, map_to_reflected_t>,
    detail::map_registered_or<
      decltype(default_to_optional_like(omni::type_t<T>{}))>>::type{};

  return map(node, type, std::move(context));
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (traits::is_optional<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_optional_t::operator()(::ryml::ConstNodeRef node, T *into,
  State context) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  assert(into);
  using target = typename detail::reflection_target<T>::type;
  constexpr auto map = typename traits::select<
    traits::case_<cp::reflect_as_root<target>::value, map_to_reflected_t>,
    detail::map_registered_or<
      decltype(default_to_optional_like(omni::type_t<T>{}))>>::type{};

  return map(node, into, std::move(context));
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename Config, typename T),
  (is_config<Config>::value && std::is_object<T>::value
    && !std::is_const<T>::value))
auto map_by_schema(const Config &options, omni::type_t<T> type,
  const ::ryml::Tree &tree) -> with_diagnostics_t<T> {
  return map_by_schema(options, type, tree.crootref());
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename Config, typename T),
  (is_config<Config>::value && std::is_object<T>::value
    && !std::is_const<T>::value))
auto map_by_schema(const Config &options, T *into, const ::ryml::Tree &tree)
  -> with_diagnostics_t<compat::monostate> {
  return map_by_schema(options, into, tree.crootref());
}

namespace detail {

// Both overloads transform syntax-error diagnostics while an empty tree
// remains alive for the configured callback.
OMNI_CONSTRAINED_TEMPLATE((typename Config), (is_config<Config>::value))
auto parse_error_diagnostics(Config &&options, std::string message)
  -> diagnostics_result_t<Config> {
  const ::ryml::Tree tree{/*node_capacity=*/0, /*arena_capacity=*/0};
  diagnostics report{
    /*config=*/{options.error_policy, {}},
    /*issues=*/{
      omni::ryml::core::make_issue(json::issue_code::parse_error,
        json::value_kind::unknown, ::ryml::ConstNodeRef{}),
    },
    /*parse_error_message=*/std::move(message),
  };

  return compat::invoke(std::forward<Config>(options).transform_diagnostics,
    tree, std::move(report));
}

} // namespace detail

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename Config, typename Record),
  (is_config<Config>::value && std::is_object<Record>::value
    && !std::is_const<Record>::value))
auto deserialize(Config &&options, omni::type_t<Record> record_type,
  std::string source)
  -> with_diagnostics_t<Record, diagnostics_result_t<Config>> {
  auto parsed = omni::ryml::core::parse(std::move(source));
  if (!parsed)
    return {
      /*value=*/compat::nullopt,
      /*diagnostics=*/detail::parse_error_diagnostics(
        std::forward<Config>(options), std::move(parsed.error())),
    };

  auto mapped = map_by_schema(options, record_type, parsed->crootref());
  return {
    /*value=*/std::move(mapped.value),
    /*diagnostics=*/compat::invoke(
      std::forward<Config>(options).transform_diagnostics,
      static_cast<const ::ryml::Tree &>(*parsed),
      std::move(mapped.diagnostics)),
  };
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename Config, typename T),
  (is_config<Config>::value && std::is_object<T>::value
    && !std::is_const<T>::value))
auto deserialize(Config &&options, T *into, std::string source)
  -> with_diagnostics_t<compat::monostate, diagnostics_result_t<Config>> {
  assert(into);
  auto parsed = omni::ryml::core::parse(std::move(source));
  if (!parsed)
    return {
      /*value=*/compat::nullopt,
      /*diagnostics=*/detail::parse_error_diagnostics(
        std::forward<Config>(options), std::move(parsed.error())),
    };

  auto mapped = map_by_schema(options, into, parsed->crootref());
  return {
    /*value=*/std::move(mapped.value),
    /*diagnostics=*/compat::invoke(
      std::forward<Config>(options).transform_diagnostics,
      static_cast<const ::ryml::Tree &>(*parsed),
      std::move(mapped.diagnostics)),
  };
}

namespace detail {

template <typename State, typename T>
struct cpp11_lambda_write {
  const T &value;

  template <typename _M, omni::reflected_entity Entity>
  ::ryml::Tree operator()(omni::meta_t<_M, Entity>) const {
    ::ryml::Tree tree;
    core::write_node(this->value, tree.rootref(), State{});
    return tree;
  }
};

template <typename Discovery,
  bool Reflect = cp::reflect_as_root<Discovery>::value,
  bool Wrapper = compat::disjunction<
    omni::ryml::detail::is_mapped_range<Discovery>,
    traits::is_optional<Discovery>>::value>
struct tree_builder_case {
  OMNI_CONSTRAINED_TEMPLATE((typename State, typename T),
    (is_write_state<State>::value))
  static ::ryml::Tree build(T &value) {
    ::ryml::Tree tree;
    write_node(value, tree.rootref(), State{});
    return tree;
  }
};

template <typename Discovery, bool Wrapper>
struct tree_builder_case<Discovery, true, Wrapper> {
  OMNI_CONSTRAINED_TEMPLATE((typename State, typename T),
    (is_write_state<State>::value))
  static ::ryml::Tree build(T &value) {
    return omni::reflected_call(
      cpp11_lambda_write<State, T>{/*value=*/value},
      omni::type_t<Discovery>{});
  }
};

template <typename Discovery>
struct tree_builder_case<Discovery, false, true> {
  OMNI_CONSTRAINED_TEMPLATE((typename State, typename T),
    (is_write_state<State>::value))
  static ::ryml::Tree build(T &value) {
    return tree_builder_case<
      typename omni::ryml::detail::discovered_range_type<Discovery>::type>
        ::template build<State>(value);
  }
};

} // namespace detail

template <typename T>
::ryml::Tree as_owning_tree(const T &value) {
  return detail::tree_builder_case<compat::remove_cvref_t<T>>
    ::template build<write_state_t<false>>(value);
}

template <typename T>
::ryml::Tree as_ref_tree(T &value) {
  return detail::tree_builder_case<compat::remove_cvref_t<T>>
    ::template build<write_state_t<true>>(value);
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
