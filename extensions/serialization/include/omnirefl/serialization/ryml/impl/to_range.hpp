#pragma once

#include <omnirefl/serialization/ryml/to_range.hpp>

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
template <typename T, bool Unique = traits::is_unique_key<T>::value>
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
    range_insertion_check<T>::template accepts<typename T::key_type &&,
      typename T::mapped_type &&> {};

inline void report_range_duplicate(::ryml::ConstNodeRef node,
  const std::vector<issue::path_segment> &path,
  bool key_value,
  node_accumulator *result) {
  const auto policy = result->context.options.error_policy.duplicates;
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

template <typename T, typename... Args>
void insert_range_value(T *into, ::ryml::ConstNodeRef,
  const mapping_context &, node_accumulator *, std::false_type,
  Args &&...args) {
  adapt::emplace(into, std::forward<Args>(args)...);
}

template <typename T, typename... Args>
void insert_range_value(T *into, ::ryml::ConstNodeRef node,
  const mapping_context &context, node_accumulator *result, std::true_type,
  Args &&...args) {
  const auto inserted = adapt::try_emplace(into, std::forward<Args>(args)...);
  if (inserted.second)
    return;

  const auto policy = context.options.error_policy.duplicates;
  if ((policy & json::duplicate_policy::last) == json::duplicate_policy::last) {
    adapt::replace(into, inserted.first, std::forward<Args>(args)...);
  }

  report_range_duplicate(node, context.path, traits::is_key_value<T>::value,
    result);
}

// Non-unique mappings retain every document member, including repeated names.
inline auto range_mapping_nodes(::ryml::ConstNodeRef node,
  node_accumulator *, std::false_type) -> decltype(node.children()) {
  return node.children();
}

// Select repeated names before decoding values, as record mapping does.
// Iterate the document again to retain the selected nodes' document order.
inline std::vector<::ryml::ConstNodeRef> range_mapping_nodes(
  ::ryml::ConstNodeRef node, node_accumulator *result, std::true_type) {
  std::map<compat::string_view, ::ryml::ConstNodeRef> selected;
  const auto policy = result->context.options.error_policy.duplicates;
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

    const auto context = result->child_context({
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

template <typename T>
void read_range(::ryml::ConstNodeRef node, T *into,
  node_accumulator *result, std::false_type) {
  std::size_t index = 0;
  for (const auto child : node.children()) {
    if (result->stopped())
      break;

    const auto context = result->child_context({
      /*field=*/{},
      /*index=*/index++,
    });

    compat::range_value_t<T> element{};
    auto mapped = core::read_node(child, &element, context);
    const bool success = bool(mapped.value);
    result->append(std::move(mapped));
    if (success)
      insert_range_value(into, child, context, result,
        traits::is_unique_key<T>{}, std::move(element));
  }
}

template <typename T>
void read_range(::ryml::ConstNodeRef node, T *into,
  node_accumulator *result, std::true_type) {
  static_assert(traits::is_string<typename T::key_type>::value,
    "to_range requires string-adaptable keys for JSON/YAML objects");

  using key_type = typename T::key_type;
  using storage_type = compat::remove_cvref_t<decltype(adapt::as_string(
    std::declval<const key_type &>()))>;

  static_assert(std::is_same<char, typename storage_type::value_type>::value,
    "Ryml object keys require char text from the string adapter");

  using view_type = compat::basic_string_view<typename storage_type::value_type,
    typename storage_type::traits_type>;

  const auto nodes = range_mapping_nodes(node, result,
    traits::is_unique_key<T>{});

  for (const auto child : nodes) {
    if (result->stopped())
      break;

    const auto context = result->child_context({
      /*field=*/child.key(),
      /*index=*/compat::nullopt,
    });

    typename T::key_type key{};
    const auto name = child.key();
    adapt::assign_string(&key, view_type{name.str, name.len});
    typename T::mapped_type value{};
    auto mapped = core::read_node(child, &value, context);
    const bool success = bool(mapped.value);
    result->append(std::move(mapped));
    if (success)
      insert_range_value(into, child, context, result,
        traits::is_unique_key<T>{}, std::move(key), std::move(value));
  }
}

} // namespace detail

namespace mapping {

template <typename T>
auto to_range_t::operator()(::ryml::ConstNodeRef node,
  T *into,
  const omni::ryml::detail::mapping_context &context) const
  -> compat::enable_if_t<omni::ryml::detail::is_mapped_range<T>::value,
    with_diagnostics<compat::monostate, diagnostics<false>>> {
  static_assert(compat::is_invocable<const traits::cp::clear<T> &, T *>::value,
    "to_range requires an adapted clear operation");

  static_assert(omni::ryml::detail::supports_range_insertion<T>::value,
    "to_range requires emplace, or try_emplace and replace for unique keys");

  auto result = omni::ryml::detail::make_node_accumulator(context, node);
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

} // namespace mapping
} // namespace ryml
} // namespace omni
