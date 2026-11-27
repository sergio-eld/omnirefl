#pragma once

#include <omnirefl/functional.hpp>
#include <omnirefl/reflected_scope.hpp>
#include <omnirefl/serialization/ryml/impl/shared.hpp>
#include <omnirefl/serialization/ryml/with_diagnostics.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <c4/format.hpp>

#include <algorithm>
#include <cassert>
#include <iterator>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {
namespace detail {

// Ad hoc: traversal selects a writer before its terminal overload can run.
// The depth sentinel lacks the state interface required by `.select_mapping`.
// REFACTORME: [high] Consolidate writer selection and depth termination in
// traversal, then remove these separate selection overloads.
template <typename T>
auto select_write_mapping(const depth_exceeded &, omni::type_t<T>)
  -> with_depth_exceeded_operator {
  return {};
}

OMNI_CONSTRAINED_TEMPLATE((typename State, typename T),
  (is_write_state<State>::value))
auto select_write_mapping(const State &state, omni::type_t<T> type)
  -> decltype(compat::invoke(state.select_mapping, state.path(), type)) {
  return compat::invoke(state.select_mapping, state.path(), type);
}

constexpr bool resolve_keep_nulls(bool keep, bool) {
  return keep;
}

constexpr bool resolve_keep_nulls(compat::monostate, bool configured) {
  return configured;
}

OMNI_CONSTRAINED_TEMPLATE((typename T), (!traits::is_optional<T>::value))
bool is_empty_optional(const T &) { return false; }

OMNI_CONSTRAINED_TEMPLATE((typename T), (traits::is_optional<T>::value))
bool is_empty_optional(const T &value) {
  return !adapt::has_value(value)
    || is_empty_optional(adapt::get_value(value));
}

template <typename State>
struct cpp11_write_member {
  ::ryml::NodeRef &node;
  State context;
  bool keep_null_fields;

  template <typename Record, typename _F>
  void operator()(omni::data_member_binding_t<Record, _F> field) const {
    const auto &value = field.value();
    if (!this->keep_null_fields && is_empty_optional(value))
      return;

    auto child = this->node.append_child();
    child.set_key_serialized(field.name());
    const auto state = this->context.at(child)
      .template field<compat::remove_cvref_t<Record>>(field.as_meta());
    const auto mapping = select_write_mapping(state,
      omni::type_t<typename omni::data_member_meta_t<_F>::type>{});
    mapping(state, value);
  }
};

} // namespace detail

namespace reflected_scope {
namespace detail {

// The binding types the record's binding would expose, without its tuple.
template <typename Record, typename Fields>
struct record_member_bindings;

template <typename Record, typename... _F>
struct record_member_bindings<Record,
  std::tuple<omni::data_member_meta_t<_F>...>> {
  using type = std::tuple<omni::data_member_binding_t<Record, _F>...>;
};

template <typename Write, typename Record, typename... Binding>
void write_record_members(const Write &write, Record &value,
  omni::type_t<std::tuple<Binding...>>) {
  // Initializer-list evaluation preserves member order before C++17.
  const int written[] = {0, (write(Binding{value}), 0)...};
  static_cast<void>(written);
  static_cast<void>(value);
}

} // namespace detail

template <typename Policy>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<omni::is_reflected_record<T>,
    is_write_state<State>>::value))
auto map_record_t<Policy>::operator()(State context,
  const T &value) const -> ::ryml::NodeRef {
  auto node = context.node();
  assert(mapping::valid_record_policy(
    compat::apply(detail::record_field_names{},
      omni::meta_for<T>::accessible_data_members()), this->policy));
  node.set_map();
  // Ad hoc: retain the original predicate construction and filter constraints
  // on an empty tuple, avoiding the costly tuple of every field binding.
  static_cast<void>(std::tuple<>{}
    | omni::fn::filter(this->policy.filter_fields));
  const auto state = omni::ryml::detail::state_schema<T>::bind(context);
  detail::write_record_members(
    omni::ryml::detail::cpp11_write_member<decltype(state)>{
      /*node=*/node,
      /*context=*/state,
      /*keep_null_fields=*/omni::ryml::detail::resolve_keep_nulls(
        this->policy.keep_nulls, context.config.keep_null_fields),
    },
    value,
    // REFACTORME: [high] Slop ad hoc: reconstruct and filter binding types
    // because metadata traversal exposes a tuple whose construction/indexing
    // causes wide-record compile growth. Replace this nested type plumbing
    // with direct field-pack traversal that preserves policy filtering.
    omni::type_t<typename detail::filter_fields<
      decltype(std::declval<Policy>().filter_fields),
      typename detail::record_member_bindings<const T,
        typename omni::meta_for<T>::accessible_data_members_t>::type>::type>{});
  return node;
}

template <typename Policy>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<omni::is_reflected_enum<T>,
    is_write_state<State>>::value))
auto map_enum_t<Policy>::operator()(State context, T value) const
  -> ::ryml::NodeRef {
  auto node = context.node();
  constexpr auto enumerators = omni::meta_for<T>::enumerators();
  for (const auto &enumerator : enumerators)
    if (value == enumerator.first) {
      node.set_serialized(c4::to_csubstr(enumerator.second));
      node.set_val_style(::ryml::VAL_DQUO);
      return node;
    }
  return node;
}

} // namespace reflected_scope

namespace detail {

// The int/long tag prefers size()-based reservation over the unsized fallback.
template <typename Range>
auto reserve_range_nodes(::ryml::Tree *tree, const Range &value, int)
  -> decltype(static_cast<::ryml::id_type>(value.size()), void()) {
  const auto required = tree->size()
    + static_cast<::ryml::id_type>(value.size());
  // Nested ranges share storage; retain geometric growth between reserves.
  if (required > tree->capacity())
    tree->reserve(std::max(required, 2 * tree->capacity()));
}

template <typename Range>
void reserve_range_nodes(::ryml::Tree *, const Range &, long) {}

template <bool KeyValue>
struct write_range_case;

template <>
struct write_range_case<false> {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (is_write_state<State>::value))
  static void apply(State context, const T &value, ::ryml::NodeRef node,
    bool keep_nulls) {
    using element_type = compat::range_value_t<T>;
    node.set_seq();
    reserve_range_nodes(node.tree(), value, /*overload_priority=*/0);
    // Separate iterator/sentinel declarations also support non-common ranges
    // before C++17 permitted them in range-based for loops.
    using std::begin;
    using std::end;
    auto current = begin(value);
    const auto last = end(value);
    for (; current != last; ++current) {
      const element_type &element = *current;
      if (!keep_nulls && is_empty_optional(element))
        continue;

      const auto state = context.at(node.append_child()).element();
      const auto mapping = select_write_mapping(state,
        omni::type_t<element_type>{});
      mapping(state, element);
    }
  }
};

template <>
struct write_range_case<true> {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (is_write_state<State>::value))
  static void apply(State context, const T &value, ::ryml::NodeRef node,
    bool keep_nulls) {
    static_assert(traits::is_string_like<traits::key_type_t<T>>::value,
      "map_range requires string-adaptable keys for JSON/YAML objects");

    node.set_map();
    reserve_range_nodes(node.tree(), value, /*overload_priority=*/0);
    using std::begin;
    using std::end;
    auto current = begin(value);
    const auto last = end(value);
    for (; current != last; ++current) {
      const auto &entry = *current;
      if (!keep_nulls && is_empty_optional(entry.second))
        continue;

      const auto &key = adapt::as_string(entry.first);
      using storage_type = decltype(adapt::as_string(entry.first));
      using text_type = compat::remove_cvref_t<storage_type>;
      static_assert(std::is_same<char, typename text_type::value_type>::value,
        "Ryml object keys require char text from the string adapter");

      using view_type = compat::basic_string_view<
        typename text_type::value_type,
        typename text_type::traits_type>;

      constexpr bool borrows_source =
        std::is_lvalue_reference<storage_type>::value
        || std::is_same<text_type, view_type>::value;

      const c4::csubstr text{key.data(), key.size()};
      auto child = node.append_child();
      // Owning key conversions are temporary, even for a reference tree.
      if (context.borrow_strings && borrows_source)
        child.set_key(text);
      else
        child.set_key_serialized(text);

      child.set_key_style(::ryml::KEY_DQUO);
      const auto state = context.at(child).element();
      const auto mapping = select_write_mapping(state,
        omni::type_t<traits::mapped_type_t<T>>{});
      mapping(state, entry.second);
    }
  }
};

} // namespace detail

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_arithmetic<T>::value && is_write_state<State>::value))
auto map_arithmetic_t::operator()(State context, const T &value) const
  -> ::ryml::NodeRef {
  const auto write = typename traits::select<
    traits::case_<std::is_same<bool, compat::remove_cv_t<T>>::value,
      map_boolean_t>,
    traits::case_<std::is_integral<T>::value, map_integral_t>,
    map_floating_point_t>::type{};

  return write(std::move(context), value);
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_integral<T>::value
    && !std::is_same<bool, compat::remove_cv_t<T>>::value
    && is_write_state<State>::value))
auto map_integral_t::operator()(State context, const T &value) const
  -> ::ryml::NodeRef {
  auto node = context.node();
  node.set_serialized(value);
  return node;
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_floating_point<T>::value && is_write_state<State>::value))
auto map_floating_point_t::operator()(State context, const T &value) const
  -> ::ryml::NodeRef {
  auto node = context.node();
  // Preserve enough significant digits to recover the original value.
  node.set_serialized(c4::fmt::real(value,
    std::numeric_limits<T>::max_digits10, c4::FTOA_FLEX));
  return node;
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_same<bool, compat::remove_cv_t<T>>::value
    && is_write_state<State>::value))
auto map_boolean_t::operator()(State context, const T &value) const
  -> ::ryml::NodeRef {
  auto node = context.node();
  // C4's plain bool conversion emits 0/1 rather than document booleans.
  node.set_serialized(c4::fmt::boolalpha(value));
  return node;
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<traits::is_string_like<T>,
    is_write_state<State>>::value))
auto map_string_like_t::operator()(State context, const T &value) const
  -> ::ryml::NodeRef {
  auto node = context.node();
  const auto &text = adapt::as_string(value);
  using storage_type = decltype(adapt::as_string(value));
  using text_type = compat::remove_cvref_t<storage_type>;
  static_assert(std::is_same<char, typename text_type::value_type>::value,
    "Ryml string mapping currently supports only char text.");

  using view_type = compat::basic_string_view<typename text_type::value_type,
    typename text_type::traits_type>;

  // Owning conversions are temporary: copy them even in a reference tree.
  constexpr bool borrows_source = std::is_lvalue_reference<storage_type>::value
    || std::is_same<text_type, view_type>::value;

  const c4::csubstr scalar{text.data(), text.size()};
  if (context.borrow_strings && borrows_source)
    node.set_val(scalar);
  else
    node.set_serialized(scalar);

  node.set_val_style(::ryml::VAL_DQUO);
  return node;
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<traits::is_optional<T>, is_write_state<State>>::value))
auto map_optional_like_t::operator()(State context, const T &value) const
  -> ::ryml::NodeRef {
  if (adapt::has_value(value)) {
    const auto state =
      detail::state_schema<typename T::value_type>::bind(context);
    const auto mapping = compat::invoke(state.select_mapping,
      state.path(), omni::type_t<typename T::value_type>{});
    return mapping(state, adapt::get_value(value));
  }

  auto node = context.node();
  node.set_val("null");
  node.set_val_style(::ryml::VAL_PLAIN);
  return node;
}

template <typename Policy>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<omni::ryml::detail::is_mapped_range<const T>,
    is_write_state<State>>::value))
auto map_range_t<Policy>::operator()(State context, const T &value) const
  -> ::ryml::NodeRef {
  auto node = context.node();
  using element_type = compat::range_value_t<T>;
  using element_reference = compat::range_reference_t<const T>;

  // Materialized non-scalar elements may own strings borrowed by the tree.
  static_assert(compat::disjunction<std::is_scalar<element_type>,
    compat::conjunction<std::is_lvalue_reference<element_reference>,
      std::is_same<element_type,
        compat::remove_cvref_t<element_reference>>>>::value,
    "Default range serialization requires lvalue references to value_type "
    "for non-scalar elements; specialize cp::from_type_t for value-producing "
    "or converting-proxy iterators.");

  detail::write_range_case<traits::is_key_value<T>::value>::apply(
    detail::state_schema<T>::bind(context),
    value,
    node,
    detail::resolve_keep_nulls(this->policy.keep_nulls,
      traits::is_key_value<T>::value ? context.config.keep_null_fields
        : context.config.keep_null_elements));

  // TODO: [medium] Consider supporting generators and temporary-producing
  // iterators by copying materialized elements' strings into tree storage.
  return node;
}

} // namespace ryml
} // namespace omni
