#pragma once

#include <omnirefl/serialization/ryml/with_diagnostics.hpp>

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
auto read_node(State context, ::ryml::ConstNodeRef node, omni::type_t<T> type)
  -> with_diagnostics_t<T, diagnostics> {
  const auto mapping = compat::invoke(context.map_node, context, type);
  return omni::ryml::detail::map_node_to_value(std::move(context),
    mapping, node, type);
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (is_read_state<State>::value))
auto read_node(State context, ::ryml::ConstNodeRef node, T *into)
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  auto result = omni::ryml::detail::make_node_accumulator(context);
  if (context.limit_exceeded())
    result.result.value.reset();
  else {
    const auto mapping =
      compat::invoke(context.map_node, context, omni::type_t<T>{});

    result.append(omni::ryml::detail::read_into(std::move(context),
      mapping,
      node,
      into,
      /*priority=*/0));
  }

  return std::move(result).finish();
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (is_write_state<State>::value))
auto write_node(State context, const T &value) -> ::ryml::NodeRef {
  const auto mapping =
    compat::invoke(context.map_node, context, omni::type_t<T>{});
  return mapping(std::move(context), value);
}

namespace detail {

// A callable alternative for non-reflected routes in the discovery switch.
struct map_selected {
  template <typename State, typename Destination>
  auto operator()(State context, ::ryml::ConstNodeRef node,
    Destination destination) const
    -> decltype(read_node(std::move(context), node, destination)) {
    return read_node(std::move(context), node, destination);
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
auto map_to_fundamental_t::operator()(State context,
  ::ryml::ConstNodeRef node,
  omni::type_t<T> type) const
  -> with_diagnostics_t<T, diagnostics> {
  return read_node(std::move(context), node, type);
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_arithmetic<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_fundamental_t::operator()(State context,
  ::ryml::ConstNodeRef node,
  T *into) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  assert(into);
  return read_node(std::move(context), node, into);
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (traits::is_string_like<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_string_like_t::operator()(State context,
  ::ryml::ConstNodeRef node,
  omni::type_t<T> type) const
  -> with_diagnostics_t<T, diagnostics> {
  return read_node(std::move(context), node, type);
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (traits::is_string_like<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_string_like_t::operator()(State context,
  ::ryml::ConstNodeRef node,
  T *into) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  assert(into);
  return read_node(std::move(context), node, into);
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
    return core::read_node(this->context, this->node, omni::type_t<T>{});
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
    return core::read_node(this->context, this->node, &this->into);
  }
};

} // namespace detail

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_object<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_reflected_t::operator()(State context,
  ::ryml::ConstNodeRef node,
  omni::type_t<T>) const
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
auto map_to_reflected_t::operator()(State context,
  ::ryml::ConstNodeRef node,
  T *into) const
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
auto map_to_sequence_t::operator()(State context,
  ::ryml::ConstNodeRef node,
  omni::type_t<T> type) const
  -> with_diagnostics_t<T, diagnostics> {
  using target = typename detail::reflection_target<T>::type;
  constexpr auto map = typename traits::select<
    traits::case_<cp::reflect_as_root<target>::value, map_to_reflected_t>,
    detail::map_selected>::type{};

  return map(std::move(context), node, type);
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (omni::ryml::detail::is_mapped_range<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_sequence_t::operator()(State context,
  ::ryml::ConstNodeRef node,
  T *into) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  assert(into);
  using target = typename detail::reflection_target<T>::type;
  constexpr auto map = typename traits::select<
    traits::case_<cp::reflect_as_root<target>::value, map_to_reflected_t>,
    detail::map_selected>::type{};

  return map(std::move(context), node, into);
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (traits::is_optional<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_optional_t::operator()(State context,
  ::ryml::ConstNodeRef node,
  omni::type_t<T> type) const
  -> with_diagnostics_t<T, diagnostics> {
  using target = typename detail::reflection_target<T>::type;
  constexpr auto map = typename traits::select<
    traits::case_<cp::reflect_as_root<target>::value, map_to_reflected_t>,
    detail::map_selected>::type{};

  return map(std::move(context), node, type);
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (traits::is_optional<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_optional_t::operator()(State context,
  ::ryml::ConstNodeRef node,
  T *into) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  assert(into);
  using target = typename detail::reflection_target<T>::type;
  constexpr auto map = typename traits::select<
    traits::case_<cp::reflect_as_root<target>::value, map_to_reflected_t>,
    detail::map_selected>::type{};

  return map(std::move(context), node, into);
}

} // namespace core

namespace detail {

// Syntax errors still reach the configured renderer while an empty tree lives.
template <typename Config>
auto parse_error_diagnostics(Config &&options, std::string message)
  -> diagnostics_result_t<Config> {
  const ::ryml::Tree tree{/*node_capacity=*/0, /*arena_capacity=*/0};
  diagnostics report{
    /*config=*/{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      options.error_policy, {}},
    /*issues=*/{
      core::make_issue(json::issue_code::parse_error,
        json::value_kind::unknown, ::ryml::ConstNodeRef{}),
    },
    /*parse_error_message=*/std::move(message),
  };

  return compat::invoke(std::forward<Config>(options).transform_diagnostics,
    tree, std::move(report));
}

template <typename T, typename Config, typename MapNode, typename Destination>
auto deserialize(Config &&options, const MapNode &map_node,
  Destination destination, std::string source)
  -> with_diagnostics_t<T, diagnostics_result_t<Config>> {
  auto parsed = core::parse(std::move(source));
  if (!parsed)
    return {
      /*value=*/compat::nullopt,
      /*diagnostics=*/detail::parse_error_diagnostics(
        std::forward<Config>(options), std::move(parsed.error())),
    };

  const map_by_schema_t<Config &, const MapNode &> map{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
    /*base=*/{},
#endif
    options, map_node};
  auto mapped = map(destination, parsed->crootref());

  return {
    /*value=*/std::move(mapped.value),
    /*diagnostics=*/compat::invoke(
      std::forward<Config>(options).transform_diagnostics,
      static_cast<const ::ryml::Tree &>(*parsed),
      std::move(mapped.diagnostics)),
  };
}

} // namespace detail

namespace core {
namespace detail {

template <typename State, typename T>
struct cpp11_lambda_write {
  State context;
  const T &value;

  template <typename _M, omni::reflected_entity Entity>
  void operator()(omni::meta_t<_M, Entity>) const {
    core::write_node(this->context, this->value);
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
  static void build(State context, T &value) {
    write_node(std::move(context), value);
  }
};

template <typename Discovery, bool Wrapper>
struct tree_builder_case<Discovery, true, Wrapper> {
  OMNI_CONSTRAINED_TEMPLATE((typename State, typename T),
    (is_write_state<State>::value))
  static void build(State context, T &value) {
    return omni::reflected_call(
      cpp11_lambda_write<State, T>{/*context=*/std::move(context),
        /*value=*/value},
      omni::type_t<Discovery>{});
  }
};

template <typename Discovery>
struct tree_builder_case<Discovery, false, true> {
  OMNI_CONSTRAINED_TEMPLATE((typename State, typename T),
    (is_write_state<State>::value))
  static void build(State context, T &value) {
    return tree_builder_case<
      typename omni::ryml::detail::discovered_range_type<Discovery>::type>
        ::build(std::move(context), value);
  }
};

} // namespace detail
} // namespace core

template <typename MapNode>
template <typename T>
::ryml::Tree as_owning_tree_t<MapNode>::operator()(const T &value) const {
  ::ryml::Tree tree;
  core::detail::tree_builder_case<compat::remove_cvref_t<T>>
    ::build(write_state_t<detail::write_state_config<false,
      compat::remove_cvref_t<MapNode>, compat::remove_cvref_t<T>>>{
      this->map_node, &tree, tree.root_id()}, value);
  return tree;
}

template <typename MapNode>
template <typename T>
::ryml::Tree as_ref_tree_t<MapNode>::operator()(T &value) const {
  ::ryml::Tree tree;
  core::detail::tree_builder_case<compat::remove_cvref_t<T>>
    ::build(write_state_t<detail::write_state_config<true,
      compat::remove_cvref_t<MapNode>, compat::remove_cvref_t<T>>>{
      this->map_node, &tree, tree.root_id()}, value);
  return tree;
}

template <typename MapNode>
template <typename Value>
std::string serialize_t<MapNode>::operator()(const Value &value) const {
  const auto tree = as_ref_tree_t<const MapNode &>{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
    /*base=*/{},
#endif
    this->map_node}(value);

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
