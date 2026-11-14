#pragma once

#include <omnirefl/serialization/ryml/mapping/sequence.hpp>

#include <map>
#include <utility>
#include <vector>

namespace omni {
namespace ryml {
namespace detail {

OMNI_CONSTRAINED_TEMPLATE((typename T), (!traits::is_optional<T>::value))
bool is_null(const T &) {
  return false;
}

OMNI_CONSTRAINED_TEMPLATE((typename T), (traits::is_optional<T>::value))
bool is_null(const T &value) {
  return !adapt::has_value(value) || is_null(adapt::get_value(value));
}

template <typename T, typename Config>
void write_range(const T &value, ::ryml::NodeRef node,
  mapping_state<Config> context, std::false_type) {
  using element_type = compat::range_value_t<T>;
  node.set_seq();
  // Separate iterator/sentinel declarations also support non-common ranges
  // before C++17 permitted them in range-based for loops.
  using std::begin;
  using std::end;
  auto current = begin(value);
  const auto last = end(value);
  std::size_t index = 0;
  for (; current != last; ++current) {
    const element_type &element = *current;
    if (!is_null(element)) {
      const auto child_state = context.child({
        /*field=*/{},
        /*index=*/index++,
      });

      core::write_node(element, node.append_child(), child_state);
    }
  }
}

template <typename T, typename Config>
void write_range(const T &value, ::ryml::NodeRef node,
  mapping_state<Config> context, std::true_type) {
  static_assert(traits::is_string<traits::key_type_t<T>>::value,
    "from_range requires string-adaptable keys for JSON/YAML objects");

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

    using view_type = compat::basic_string_view<typename text_type::value_type,
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
    const auto child_state = context.child({
      /*field=*/text,
      /*index=*/compat::nullopt,
    });

    core::write_node(entry.second, child, child_state);
  }
}

} // namespace detail

template <typename T,
  typename Config,
  compat::enable_if_t<omni::ryml::detail::is_mapped_range<const T>::value,
    int>>
void from_range_t::operator()(const T &value,
  ::ryml::NodeRef node,
  mapping_state<Config> context) const {
  using element_type = compat::range_value_t<T>;
  using element_reference = compat::range_reference_t<const T>;

  // Materialized non-scalar elements may own strings borrowed by the tree.
  static_assert(compat::disjunction<std::is_scalar<element_type>,
    compat::conjunction<std::is_lvalue_reference<element_reference>,
      std::is_same<element_type,
        compat::remove_cvref_t<element_reference>>>>::value,
    "Default range serialization requires lvalue references to value_type "
    "for non-scalar C++ elements; specialize cp::from_type for value-producing "
    "or converting-proxy iterators.");

  detail::write_range(value, node, context, traits::is_key_value<T>{});

  // TODO: [medium] Consider supporting generators and temporary-producing
  // iterators by copying materialized elements' strings into tree storage.
}

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

template <typename Config>
void report_range_duplicate(::ryml::ConstNodeRef node,
  mapping_path path,
  bool key_value,
  node_accumulator<Config> *result) {
  const auto policy = result->state.config.error_policy.duplicates;
  const bool allowed = (policy
    & (json::duplicate_policy::allow | json::duplicate_policy::last))
    != json::duplicate_policy::reject;

  if (allowed
    && (policy & json::duplicate_policy::warn) != json::duplicate_policy::warn)
    return;

  result->report(make_issue(key_value ? json::issue_code::duplicate_field
                                     : json::issue_code::duplicate_element,
    json::value_kind::unknown,
    node,
    path,
    /*warning=*/allowed));
}

template <typename T, typename Config, typename... Args>
void insert_range_value(T *into, ::ryml::ConstNodeRef,
  mapping_state<Config>, node_accumulator<Config> *, std::false_type,
  Args &&...args) {
  adapt::emplace(into, std::forward<Args>(args)...);
}

template <typename T, typename Config, typename... Args>
void insert_range_value(T *into, ::ryml::ConstNodeRef node,
  mapping_state<Config> context, node_accumulator<Config> *result,
  std::true_type,
  Args &&...args) {
  const auto inserted = adapt::try_emplace(into, std::forward<Args>(args)...);
  if (inserted.second)
    return;

  const auto policy = context.config.error_policy.duplicates;
  if ((policy & json::duplicate_policy::last) == json::duplicate_policy::last) {
    adapt::replace(into, inserted.first, std::forward<Args>(args)...);
  }

  report_range_duplicate(node, context.path, traits::is_key_value<T>::value,
    result);
}

// Non-unique mappings retain every document member, including repeated names.
template <typename Config>
auto range_mapping_nodes(::ryml::ConstNodeRef node,
  node_accumulator<Config> *, std::false_type) -> decltype(node.children()) {
  return node.children();
}

// Select repeated names before decoding values, as record mapping does.
// Iterate the document again to retain the selected nodes' document order.
template <typename Config>
std::vector<::ryml::ConstNodeRef> range_mapping_nodes(
  ::ryml::ConstNodeRef node, node_accumulator<Config> *result, std::true_type) {
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

    if ((policy & json::duplicate_policy::last) == json::duplicate_policy::last)
      inserted.first->second = child;

    const auto context = result->state.child({
      /*field=*/name,
      /*index=*/compat::nullopt,
    });

    report_range_duplicate(child, context.path, /*key_value=*/true, result);
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

template <typename T, typename Config>
void read_range(::ryml::ConstNodeRef node, T *into,
  node_accumulator<Config> *result, std::false_type) {
  std::size_t index = 0;
  for (const auto child : node.children()) {
    if (result->stopped())
      break;

    const auto context = result->state.child({
      /*field=*/{},
      /*index=*/index++,
    });

    compat::range_value_t<T> element{};
    auto mapped = core::read_node(child, &element, context);
    const bool success = bool(mapped.value);
    result->append(std::move(mapped));
    if (success)
      insert_range_value(into, child, context, result,
        traits::has_unique_key<T>{}, std::move(element));
  }
}

template <typename T, typename Config>
void read_range(::ryml::ConstNodeRef node, T *into,
  node_accumulator<Config> *result, std::true_type) {
  static_assert(traits::is_string<traits::key_type_t<T>>::value,
    "to_range requires string-adaptable keys for JSON/YAML objects");

  using key_type = traits::key_type_t<T>;
  using storage_type = compat::remove_cvref_t<decltype(adapt::as_string(
    std::declval<const key_type &>()))>;

  static_assert(std::is_same<char, typename storage_type::value_type>::value,
    "Ryml object keys require char text from the string adapter");

  using view_type = compat::basic_string_view<typename storage_type::value_type,
    typename storage_type::traits_type>;

  const auto nodes = range_mapping_nodes(node, result,
    traits::has_unique_key<T>{});

  for (const auto child : nodes) {
    if (result->stopped())
      break;

    const auto context = result->state.child({
      /*field=*/child.key(),
      /*index=*/compat::nullopt,
    });

    traits::key_type_t<T> key{};
    const auto name = child.key();
    adapt::assign_string(&key, view_type{name.str, name.len});
    traits::mapped_type_t<T> value{};
    auto mapped = core::read_node(child, &value, context);
    const bool success = bool(mapped.value);
    result->append(std::move(mapped));
    if (success)
      insert_range_value(into, child, context, result,
        traits::has_unique_key<T>{}, std::move(key), std::move(value));
  }
}

} // namespace detail

template <typename T, typename Config>
auto to_range_t::operator()(::ryml::ConstNodeRef node,
  T *into,
  mapping_state<Config> context) const
  -> compat::enable_if_t<omni::ryml::detail::is_mapped_range<T>::value,
    with_diagnostics<compat::monostate, diagnostics>> {
  static_assert(compat::is_invocable<const traits::cp::clear<T> &, T *>::value,
    "to_range requires an adapted clear operation");

  static_assert(omni::ryml::detail::supports_range_insertion<T>::value,
    "to_range requires emplace, or try_emplace and replace for unique keys");

  auto result = omni::ryml::detail::make_node_accumulator(context);
  constexpr bool key_value = traits::is_key_value<T>::value;
  if (key_value ? !node.is_map() : !node.is_seq()) {
    result.report(omni::ryml::detail::make_issue(
      json::issue_code::unexpected_kind,
      key_value ? json::value_kind::mapping : json::value_kind::sequence,
      node,
      context.path));

    return std::move(result).finish();
  }

  adapt::clear(into);
  omni::ryml::detail::read_range(node, into, &result,
    traits::is_key_value<T>{});

  return std::move(result).finish();
}

} // namespace ryml
} // namespace omni
