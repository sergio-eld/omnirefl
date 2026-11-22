#pragma once

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

      auto mapped = core::read_node(result->state.element(), child,
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
      auto mapped = core::read_node(result->state.element(), child,
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

namespace detail {

template <typename T>
bool is_null(const T &value);

template <bool Optional>
struct is_null_case {
  template <typename T>
  static bool get(const T &) { return false; }
};

template <>
struct is_null_case<true> {
  template <typename T>
  static bool get(const T &value) {
    return !adapt::has_value(value) || is_null(adapt::get_value(value));
  }
};

template <typename T>
bool is_null(const T &value) {
  return is_null_case<traits::is_optional<T>::value>::get(value);
}

template <bool KeyValue>
struct write_range_case;

template <>
struct write_range_case<false> {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (is_write_state<State>::value))
  static void apply(State context, const T &value, ::ryml::NodeRef node) {
    using element_type = compat::range_value_t<T>;
    node.set_seq();
    // Separate iterator/sentinel declarations also support non-common ranges
    // before C++17 permitted them in range-based for loops.
    using std::begin;
    using std::end;
    auto current = begin(value);
    const auto last = end(value);
    for (; current != last; ++current) {
      const element_type &element = *current;
      if (!is_null(element)) {
        core::write_node(context.element(), element, node.append_child());
      }
    }
  }
};

template <>
struct write_range_case<true> {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (is_write_state<State>::value))
  static void apply(State context, const T &value, ::ryml::NodeRef node) {
    static_assert(traits::is_string_like<traits::key_type_t<T>>::value,
      "map_range requires string-adaptable keys for JSON/YAML objects");

    node.set_map();
    using std::begin;
    using std::end;
    auto current = begin(value);
    const auto last = end(value);
    for (; current != last; ++current) {
      const auto &entry = *current;
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
      core::write_node(context.element(), entry.second, child);
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

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<omni::ryml::detail::is_mapped_range<const T>,
    is_write_state<State>>::value))
void map_range_t::operator()(State context,
  const T &value,
  ::ryml::NodeRef node) const {
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
    std::move(context),
    value,
    node);

  // TODO: [medium] Consider supporting generators and temporary-producing
  // iterators by copying materialized elements' strings into tree storage.
}

} // namespace ryml
} // namespace omni
