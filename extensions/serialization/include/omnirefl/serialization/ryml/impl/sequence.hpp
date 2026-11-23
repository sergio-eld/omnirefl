#pragma once

#include <omnirefl/serialization/ryml/with_diagnostics.hpp>

#include <cassert>
#include <cstddef>
#include <map>
#include <utility>
#include <vector>

namespace omni {
namespace ryml {
namespace detail {

template <typename T, typename... Args>
using unique_range_insertion = compat::conjunction<
  compat::is_invocable<const traits::cp::try_emplace<T> &, T *, Args...>,
  compat::is_invocable<const traits::cp::replace<T> &, T *,
    decltype(adapt::try_emplace(std::declval<T *>(),
      std::declval<Args>()...).first), Args...>>;

// Keep uniqueness-dependent expressions out of non-unique range checks.
template <typename T, bool Unique = traits::has_unique_key<T>::value>
struct range_insertion_check {
  template <typename... Args>
  using accepts = compat::is_invocable<const traits::cp::emplace<T> &,
    T *, Args...>;
};

template <typename T>
struct range_insertion_check<T, true> {
  template <typename... Args>
  using accepts = unique_range_insertion<T, Args...>;
};

template <typename T, bool KeyValue = traits::is_key_value<T>::value>
struct supports_range_insertion:
    range_insertion_check<T>::template accepts<compat::range_value_t<T> &&> {};

template <typename T>
struct supports_range_insertion<T, true>:
    range_insertion_check<T>::template accepts<traits::key_type_t<T> &&,
      traits::mapped_type_t<T> &&> {};

template <typename State>
void report_range_duplicate(::ryml::ConstNodeRef node,
  bool key_value,
  node_accumulator<State> *result) {
  const auto policy = result->state.config.error_policy.duplicates;
  const bool allowed = (policy
    & (json::duplicate_policy::allow | json::duplicate_policy::last))
    != json::duplicate_policy::reject;

  if (allowed
    && (policy & json::duplicate_policy::warn) != json::duplicate_policy::warn)
    return;

  result->report(core::make_issue(key_value ? json::issue_code::duplicate_field
                                        : json::issue_code::duplicate_element,
    json::value_kind::unknown,
    node,
    /*warning=*/allowed));
}

template <bool Unique>
struct insert_range_value_case;

template <>
struct insert_range_value_case<false> {
  template <typename T, typename State, typename... Args>
  static void apply(T *into, ::ryml::ConstNodeRef,
    node_accumulator<State> *, Args &&...args) {
    adapt::emplace(into, std::forward<Args>(args)...);
  }
};

template <>
struct insert_range_value_case<true> {
  template <typename T, typename State, typename... Args>
  static void apply(T *into, ::ryml::ConstNodeRef node,
    node_accumulator<State> *result, Args &&...args) {
    const auto inserted = adapt::try_emplace(into, std::forward<Args>(args)...);
    if (inserted.second)
      return;

    const auto policy = result->state.config.error_policy.duplicates;
    if ((policy & json::duplicate_policy::last)
        == json::duplicate_policy::last)
      adapt::replace(into, inserted.first, std::forward<Args>(args)...);

    report_range_duplicate(node, traits::is_key_value<T>::value, result);
  }
};

// Non-unique mappings retain every document member, including repeated names.
template <bool Unique>
struct range_mapping_nodes_case;

template <>
struct range_mapping_nodes_case<false> {
  template <typename State>
  static auto apply(::ryml::ConstNodeRef node,
    node_accumulator<State> *) -> decltype(node.children()) {
    return node.children();
  }
};

// Select repeated names before decoding values, as record mapping does.
// Iterate the document again to retain the selected nodes' document order.
template <>
struct range_mapping_nodes_case<true> {
  template <typename State>
  static std::vector<::ryml::ConstNodeRef> apply(
    ::ryml::ConstNodeRef node, node_accumulator<State> *result) {
    std::map<compat::string_view, ::ryml::ConstNodeRef> selected;
    const auto policy = result->state.config.error_policy.duplicates;
    for (const auto child : node.children()) {
      if (result->stopped())
        break;

      const auto name = child.key();
      const auto inserted = selected.emplace(
        compat::string_view{name.str, name.len}, child);

      if (inserted.second)
        continue;

      if ((policy & json::duplicate_policy::last)
        == json::duplicate_policy::last)
        inserted.first->second = child;

      report_range_duplicate(child, /*key_value=*/true, result);
    }

    std::vector<::ryml::ConstNodeRef> nodes;
    nodes.reserve(selected.size());
    for (const auto child : node.children()) {
      const auto name = child.key();
      const auto found = selected.find({name.str, name.len});
      if (found != selected.end() && found->second.id() == child.id())
        nodes.push_back(child);
    }

    return nodes;
  }
};

// Reservation is optional; custom ranges retain their insertion contract.
// Probe the usual capacity API so unrelated reserve operations are not used.
template <typename T>
auto reserve_range_storage(T *into, ::ryml::ConstNodeRef node, int)
  -> decltype(into->reserve(std::size_t{}), into->capacity(),
    into->max_size(), void()) {
  into->reserve(node.num_children());
}

template <typename T>
void reserve_range_storage(T *, ::ryml::ConstNodeRef, long) {}

template <bool KeyValue>
struct read_range_case;

template <>
struct read_range_case<false> {
  template <typename T, typename State>
  static void apply(::ryml::ConstNodeRef node, T *into,
    node_accumulator<State> *result) {
    bool reserve_pending = true;
    for (const auto child : node.children()) {
      if (result->stopped())
        break;

      auto mapped = read_node(result->state.element(), child,
        omni::type_t<compat::range_value_t<T>>{});
      if (!mapped.value || !mapped.diagnostics.issues.empty()
        || !mapped.diagnostics.parse_error_message.empty())
        result->append({
          /*value=*/mapped.value ? compat::make_optional(compat::monostate{})
            : compat::nullopt,
          /*diagnostics=*/std::move(mapped.diagnostics),
        });
      if (mapped.value) {
        // Immediate failures need no destination allocation.
        if (reserve_pending) {
          reserve_range_storage(into, node, /*priority=*/0);
          reserve_pending = false;
        }

        insert_range_value_case<traits::has_unique_key<T>::value>::apply(
          into, child, result, std::move(*mapped.value));
      }
    }
  }
};

template <>
struct read_range_case<true> {
  template <typename T, typename State>
  static void apply(::ryml::ConstNodeRef node, T *into,
    node_accumulator<State> *result) {
    static_assert(traits::is_string_like<traits::key_type_t<T>>::value,
      "to_range requires string-adaptable keys for JSON/YAML objects");

    using key_type = traits::key_type_t<T>;
    using storage_type = compat::remove_cvref_t<decltype(adapt::as_string(
      std::declval<const key_type &>()))>;

    static_assert(std::is_same<char, typename storage_type::value_type>::value,
      "Ryml object keys require char text from the string adapter");

    using view_type = compat::basic_string_view<
      typename storage_type::value_type,
      typename storage_type::traits_type>;

    const auto nodes =
      range_mapping_nodes_case<traits::has_unique_key<T>::value>::apply(
        node, result);

    for (const auto child : nodes) {
      if (result->stopped())
        break;

      const auto name = child.key();
      auto key = adapt::from_string(omni::type_t<key_type>{},
        view_type{name.str, name.len});
      auto mapped = read_node(result->state.element(), child,
        omni::type_t<traits::mapped_type_t<T>>{});
      result->append({
        /*value=*/mapped.value ? compat::make_optional(compat::monostate{})
          : compat::nullopt,
        /*diagnostics=*/std::move(mapped.diagnostics),
      });
      if (mapped.value)
        insert_range_value_case<traits::has_unique_key<T>::value>::apply(
          into, child, result, std::move(key), std::move(*mapped.value));
    }
  }
};

} // namespace detail

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<detail::is_mapped_range<T>,
    is_read_state<State>>::value))
auto to_range_t::operator()(State context,
  ::ryml::ConstNodeRef node,
  T *into) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  static_assert(compat::is_invocable<const traits::cp::clear<T> &, T *>::value,
    "to_range requires an adapted clear operation");

  static_assert(omni::ryml::detail::supports_range_insertion<T>::value,
    "to_range requires emplace, or try_emplace and replace for unique keys");

  auto result = omni::ryml::detail::make_node_accumulator(
    std::move(context));
  assert(into);
  assert(traits::is_key_value<T>::value ? node.is_map() : node.is_seq());

  adapt::clear(into);
  omni::ryml::detail::read_range_case<traits::is_key_value<T>::value>::apply(
    node, into, &result);

  return std::move(result).finish();
}

template <typename WithNodeCheck>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (detail::is_mapped_range<T>::value && is_read_state<State>::value))
auto map_range_node_t<WithNodeCheck>::operator()(State state,
  ::ryml::ConstNodeRef node, T *into) const
  -> with_diagnostics_t<compat::monostate> {
  if (state.limit_exceeded())
    return {compat::nullopt, {{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      state.config.error_policy, {}}, {}, {}}};

  if (this->with_node_check) {
    auto checked = traits::is_key_value<T>::value
      ? core::expect_object_node(node) : core::expect_array_node(node);
    if (!checked)
      return {compat::nullopt, {{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
        /*base=*/{},
#endif
        state.config.error_policy, {}}, {std::move(checked.error())}, {}}};
  }

  return to_range_t{}(std::move(state), node, into);
}

} // namespace ryml
} // namespace omni
