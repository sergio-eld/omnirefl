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
auto read_node(State context, ::ryml::ConstNodeRef node, omni::type_t<T> type)
  -> with_diagnostics_t<T, diagnostics> {
  return omni::ryml::detail::map_node_to_value(std::move(context),
    omni::ryml::reflected_scope::mapping_to_type(type), node, type);
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
      omni::ryml::reflected_scope::mapping_to_type(omni::type_t<T>{});

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
void write_node(State context, const T &value, ::ryml::NodeRef node) {
  omni::ryml::reflected_scope::mapping_from_type(omni::type_t<T>{})(
    std::move(context),
    value,
    node);
}

namespace detail {

// An explicit default keeps non-reflected entry routes independent of metadata.
template <typename T, typename Default>
OMNI_CPP14_CONSTEXPR auto registered_mapping(omni::type_t<T>,
  const Default &mapping_default)
  OMNI_CPP11_TRAILING_RETURN(decltype(omni::ryml::detail::first_available(
    cp::to_type_t<T>{}, traits::cp::unavailable{}, mapping_default))) {
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
    mapping_t, mapping_var, mapping_default);

  static_assert(std::is_trivially_copyable<decltype(mapping)>::value
      && std::is_copy_constructible<decltype(mapping)>::value,
    "Node mapping callables must be trivially copyable and copy-constructible");

  return mapping;
}

template <typename Default>
struct map_registered_or {
  template <typename T, typename State>
  auto operator()(State context,
    ::ryml::ConstNodeRef node,
    omni::type_t<T> type) const
    -> with_diagnostics_t<T, diagnostics> {
    return omni::ryml::detail::map_node_to_value(std::move(context),
      registered_mapping(type, Default{}), node, type);
  }

  template <typename T, typename State>
  auto operator()(State context, ::ryml::ConstNodeRef node, T *into) const
    -> with_diagnostics_t<compat::monostate, diagnostics> {
    auto result = omni::ryml::detail::make_node_accumulator(context);
    if (context.limit_exceeded())
      result.result.value.reset();
    else
      result.append(omni::ryml::detail::read_into(std::move(context),
        registered_mapping(omni::type_t<T>{}, Default{}),
        node,
        into,
        /*priority=*/0));

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

template <typename Default>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_arithmetic<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_fundamental_t<Default>::operator()(State context,
  ::ryml::ConstNodeRef node,
  omni::type_t<T> type) const
  -> with_diagnostics_t<T, diagnostics> {
  return detail::map_registered_or<default_to_arithmetic_t<>>{}(
    std::move(context),
    node,
    type);
}

template <typename Default>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_arithmetic<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_fundamental_t<Default>::operator()(State context,
  ::ryml::ConstNodeRef node,
  T *into) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  assert(into);
  return detail::map_registered_or<default_to_arithmetic_t<>>{}(
    std::move(context),
    node,
    into);
}

template <typename Default>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (traits::is_string_like<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_string_like_t<Default>::operator()(State context,
  ::ryml::ConstNodeRef node,
  omni::type_t<T> type) const
  -> with_diagnostics_t<T, diagnostics> {
  return detail::map_registered_or<default_to_string_like_t<>>{}(
    std::move(context),
    node,
    type);
}

template <typename Default>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (traits::is_string_like<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_string_like_t<Default>::operator()(State context,
  ::ryml::ConstNodeRef node,
  T *into) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  assert(into);
  return detail::map_registered_or<default_to_string_like_t<>>{}(
    std::move(context),
    node,
    into);
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

    return omni::ryml::detail::map_node_to_value(this->context,
      mapping,
      this->node,
      omni::type_t<T>{});
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

template <typename Default>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_object<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_reflected_t<Default>::operator()(State context,
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

template <typename Default>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_object<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_reflected_t<Default>::operator()(State context,
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

template <typename Default>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (omni::ryml::detail::is_mapped_range<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_sequence_t<Default>::operator()(State context,
  ::ryml::ConstNodeRef node,
  omni::type_t<T> type) const
  -> with_diagnostics_t<T, diagnostics> {
  using target = typename detail::reflection_target<T>::type;
  constexpr auto map = typename traits::select<
    traits::case_<cp::reflect_as_root<target>::value, map_to_reflected_t<>>,
    detail::map_registered_or<
      decltype(default_to_range(omni::type_t<T>{}))>>::type{};

  return map(std::move(context), node, type);
}

template <typename Default>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (omni::ryml::detail::is_mapped_range<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_sequence_t<Default>::operator()(State context,
  ::ryml::ConstNodeRef node,
  T *into) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  assert(into);
  using target = typename detail::reflection_target<T>::type;
  constexpr auto map = typename traits::select<
    traits::case_<cp::reflect_as_root<target>::value, map_to_reflected_t<>>,
    detail::map_registered_or<
      decltype(default_to_range(omni::type_t<T>{}))>>::type{};

  return map(std::move(context), node, into);
}

template <typename Default>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (traits::is_optional<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_optional_t<Default>::operator()(State context,
  ::ryml::ConstNodeRef node,
  omni::type_t<T> type) const
  -> with_diagnostics_t<T, diagnostics> {
  using target = typename detail::reflection_target<T>::type;
  constexpr auto map = typename traits::select<
    traits::case_<cp::reflect_as_root<target>::value, map_to_reflected_t<>>,
    detail::map_registered_or<
      decltype(default_to_optional_like(omni::type_t<T>{}))>>::type{};

  return map(std::move(context), node, type);
}

template <typename Default>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (traits::is_optional<T>::value && !std::is_const<T>::value
    && is_read_state<State>::value))
auto map_to_optional_t<Default>::operator()(State context,
  ::ryml::ConstNodeRef node,
  T *into) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  assert(into);
  using target = typename detail::reflection_target<T>::type;
  constexpr auto map = typename traits::select<
    traits::case_<cp::reflect_as_root<target>::value, map_to_reflected_t<>>,
    detail::map_registered_or<
      decltype(default_to_optional_like(omni::type_t<T>{}))>>::type{};

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
    /*config=*/{options.error_policy, {}},
    /*issues=*/{
      core::make_issue(json::issue_code::parse_error,
        json::value_kind::unknown, ::ryml::ConstNodeRef{}),
    },
    /*parse_error_message=*/std::move(message),
  };

  return compat::invoke(std::forward<Config>(options).transform_diagnostics,
    tree, std::move(report));
}

template <typename T, typename Config, typename Destination>
auto deserialize(Config &&options, Destination destination, std::string source)
  -> with_diagnostics_t<T, diagnostics_result_t<Config>> {
  auto parsed = core::parse(std::move(source));
  if (!parsed)
    return {
      /*value=*/compat::nullopt,
      /*diagnostics=*/detail::parse_error_diagnostics(
        std::forward<Config>(options), std::move(parsed.error())),
    };

  auto mapped = map_by_schema_t<Config &>{options}(
    destination, parsed->crootref());

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
  const T &value;

  template <typename _M, omni::reflected_entity Entity>
  ::ryml::Tree operator()(omni::meta_t<_M, Entity>) const {
    ::ryml::Tree tree;
    core::write_node(State{}, this->value, tree.rootref());
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
    write_node(State{}, value, tree.rootref());
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
} // namespace core

template <typename Default>
template <typename T>
::ryml::Tree as_owning_tree_t<Default>::operator()(const T &value) const {
  return core::detail::tree_builder_case<compat::remove_cvref_t<T>>
    ::template build<write_state_t<false>>(value);
}

template <typename Default>
template <typename T>
::ryml::Tree as_ref_tree_t<Default>::operator()(T &value) const {
  return core::detail::tree_builder_case<compat::remove_cvref_t<T>>
    ::template build<write_state_t<true>>(value);
}

template <typename Default>
template <typename Value>
std::string serialize_t<Default>::operator()(const Value &value) const {
  const auto tree = as_ref_tree(value);

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

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
namespace omni {
namespace ryml {
namespace core {

template <typename Default>
constexpr map_to_fundamental_t<Default> map_to_fundamental_t<Default>::_default{};

template <typename Default>
constexpr map_to_optional_t<Default> map_to_optional_t<Default>::_default{};

template <typename Default>
constexpr map_to_reflected_t<Default> map_to_reflected_t<Default>::_default{};

template <typename Default>
constexpr map_to_sequence_t<Default> map_to_sequence_t<Default>::_default{};

template <typename Default>
constexpr map_to_string_like_t<Default> map_to_string_like_t<Default>::_default{};

} // namespace core
} // namespace ryml
} // namespace omni
#endif

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
namespace omni {
namespace ryml {

template <typename Config>
const map_by_schema_t<Config> map_by_schema_t<Config>::_default{};

template <typename Config>
const deserialize_t<Config> deserialize_t<Config>::_default{};

template <typename Default>
constexpr as_owning_tree_t<Default> as_owning_tree_t<Default>::_default{};

template <typename Default>
constexpr as_ref_tree_t<Default> as_ref_tree_t<Default>::_default{};

template <typename Default>
constexpr serialize_t<Default> serialize_t<Default>::_default_storage::yaml;

template <typename Default>
constexpr serialize_t<Default> serialize_t<Default>::_default_storage::json;

} // namespace ryml
} // namespace omni
#endif
