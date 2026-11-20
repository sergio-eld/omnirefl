#pragma once

// TODO: [medium] Validate aliases_dict against other aliases and reflected
// canonical names, possibly inline when constructing a dictionary.

#include <omnirefl/serialization/ryml/impl/core_shared.hpp>
#include <omnirefl/extensions/traits.hpp>
#include <omnirefl/functional.hpp>

#include <algorithm>
#include <array>
#include <tuple>
#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {
namespace reflected_scope {
namespace detail {

// Defined after type-mapping selection is available; record headers use this
// declaration without depending on the later customization-point definitions.
OMNI_CONSTRAINED_TEMPLATE((typename Field, typename State),
  (is_read_state<State>::value))
auto map_node_to_field(State state, ::ryml::ConstNodeRef node, Field field)
  -> with_diagnostics_t<compat::monostate>;

} // namespace detail
} // namespace reflected_scope

namespace core {

// Mapper bodies delegate recursively through these dispatchers, whose
// definitions live in the umbrella's implementation header. The first
// declaration supplies SFINAE defaults; later declarations omit them.
OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
  (is_read_state<State>::value))
auto read_node(State state, ::ryml::ConstNodeRef node, omni::type_t<T> type)
  -> with_diagnostics_t<T, diagnostics>;

OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
  (is_read_state<State>::value))
auto read_node(State state, ::ryml::ConstNodeRef node, T *into)
  -> with_diagnostics_t<compat::monostate, diagnostics>;

OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
  (is_write_state<State>::value))
void write_node(State state, const T &value, ::ryml::NodeRef node);

} // namespace core

namespace detail {

// Before constexpr if, tag dispatch selects among different result types.
#if !defined(__cpp_if_constexpr) || 201606L > __cpp_if_constexpr
template <bool FirstAvailable, bool SecondAvailable>
struct first_available_case {
  template <typename First, typename Second, typename Third>
  static constexpr First get(const First &first, const Second &,
    const Third &) { return first; }
};

template <>
struct first_available_case<false, true> {
  template <typename First, typename Second, typename Third>
  static constexpr Second get(const First &, const Second &second,
    const Third &) { return second; }
};

template <>
struct first_available_case<false, false> {
  template <typename First, typename Second, typename Third>
  static constexpr Third get(const First &, const Second &,
    const Third &third) { return third; }
};
#endif

// Return the first registered mapping, or the default (possibly unavailable).
template <typename First, typename Second, typename Third>
constexpr auto first_available(const First &first, const Second &second,
  const Third &third)
  OMNI_CPP11_TRAILING_RETURN(decltype(first_available_case<
    !std::is_base_of<traits::cp::unavailable, First>::value,
    !std::is_base_of<traits::cp::unavailable, Second>::value>
      ::get(first, second, third))) {
#if defined(__cpp_if_constexpr) && 201606L <= __cpp_if_constexpr
  if constexpr (!std::is_base_of<traits::cp::unavailable, First>::value)
    return first;
  else if constexpr (!std::is_base_of<traits::cp::unavailable, Second>::value)
    return second;
  else
    return third;
#else
  return first_available_case<
    !std::is_base_of<traits::cp::unavailable, First>::value,
    !std::is_base_of<traits::cp::unavailable, Second>::value>
      ::get(first, second, third);
#endif
}

// Strings remain scalar mappings even though they also model input ranges.
template <typename T>
struct is_mapped_range:
    compat::conjunction<compat::is_input_range<T>,
      compat::negation<traits::is_string_like<compat::remove_cvref_t<T>>>> {};

template <typename T,
  bool Optional = traits::is_optional<T>::value,
  bool KeyValue = traits::is_key_value<T>::value>
struct discovered_range_type {
  using type = compat::remove_cvref_t<compat::range_value_t<T>>;
};

template <typename T, bool KeyValue>
struct discovered_range_type<T, true, KeyValue> {
  using type = compat::remove_cvref_t<typename T::value_type>;
};

template <typename T>
struct discovered_range_type<T, false, true> {
  using type = compat::remove_cvref_t<traits::mapped_type_t<T>>;
};

struct alias_entry {
  compat::string_view alias;
  compat::string_view name;
};

// Compare bounded character buffers directly; view comparison operators
// lack C++11 constant evaluation. Lengths also preserve embedded NULs.
constexpr bool alias_less(const char *left,
  std::size_t left_size,
  const char *right,
  std::size_t right_size,
  std::size_t index = 0) {
  return index == right_size ? false
    : index == left_size ? true
    : static_cast<unsigned char>(left[index])
        == static_cast<unsigned char>(right[index])
      ? alias_less(left, left_size, right, right_size, index + 1)
      : static_cast<unsigned char>(left[index])
        < static_cast<unsigned char>(right[index]);
}

constexpr bool alias_less(compat::string_view left, compat::string_view right) {
  return alias_less(left.data(), left.size(), right.data(), right.size());
}

// Views borrow the strings supplied to aliases_dict; keep their storage alive.
template <std::size_t Size>
struct name_by_alias_lookup {
  // aliases_dict sorts these entries; users must not modify them afterward.
  std::array<alias_entry, Size> _entries;

  compat::optional<compat::string_view> operator()(
    compat::string_view alias) const {
    const auto found = std::lower_bound(this->_entries.begin(),
      this->_entries.end(),
      alias,
      [](const alias_entry &entry, compat::string_view alias) {
        return entry.alias < alias;
      });

    return found != this->_entries.end() && found->alias == alias
      ? compat::optional<compat::string_view>{found->name}
      : compat::nullopt;
  }
};

// C++11 lambda equivalent: unpack the canonical name and its aliases.
struct cpp11_invert_alias_group {
  template <typename... Aliases>
  OMNI_CPP14_CONSTEXPR auto operator()(compat::string_view name,
    const Aliases &...aliases) const
    -> decltype(std::make_tuple(
      alias_entry{compat::string_view{aliases}, name}...)) {
    return std::make_tuple(alias_entry{compat::string_view{aliases}, name}...);
  }
};

// C++11 cannot mutate arrays in constant evaluation. Build a new index order
// at each insertion, then copy entries only when materializing the result.
// Built-in arrays allow C++11 constexpr access; one slot handles Size=0.
template <std::size_t Size>
struct alias_buffer {
  alias_entry values[Size ? Size : 1];
};

template <std::size_t Size>
struct alias_order {
  std::size_t values[Size ? Size : 1];
};

template <std::size_t... Index>
constexpr alias_order<sizeof...(Index)> alias_indices(
  compat::index_sequence<Index...>) {
  return {{Index...}};
}

template <std::size_t Size>
constexpr std::size_t alias_position(const alias_buffer<Size> &entries,
  const alias_order<Size> &order,
  std::size_t next,
  std::size_t position) {
  return position == 0
      || !alias_less(entries.values[next].alias,
        entries.values[order.values[position - 1]].alias)
    ? position
    : alias_position(entries, order, next, position - 1);
}

template <std::size_t Size, std::size_t... Index>
constexpr alias_order<Size> insert_alias_index(const alias_order<Size> &order,
  std::size_t next,
  std::size_t position,
  compat::index_sequence<Index...>) {
  return {{(Index == position ? next
    : Index > position && Index <= next ? order.values[Index - 1]
    : order.values[Index])...}};
}

template <std::size_t Size>
constexpr alias_order<Size> insertion_sort(const alias_buffer<Size> &entries,
  const alias_order<Size> &order,
  std::size_t next = 1) {
  return next >= Size ? order
    : insertion_sort(entries,
        insert_alias_index(order,
          next,
          alias_position(entries, order, next, next),
          compat::make_index_sequence<Size>{}),
        next + 1);
}

template <std::size_t Size, std::size_t... Index>
constexpr std::array<alias_entry, Size> materialize_alias_array(
  const alias_buffer<Size> &entries,
  const alias_order<Size> &order,
  compat::index_sequence<Index...>) {
  return {{entries.values[order.values[Index]]...}};
}

template <std::size_t Size>
constexpr std::array<alias_entry, Size> sort_alias_entries(
  const alias_buffer<Size> &entries) {
  return materialize_alias_array(entries,
    insertion_sort(entries,
      alias_indices(compat::make_index_sequence<Size>{})),
    compat::make_index_sequence<Size>{});
}

// C++11 lambda equivalent: materialize and sort the flattened fixed-size table.
struct cpp11_sorted_aliases {
  template <typename... Entries>
  constexpr name_by_alias_lookup<sizeof...(Entries)> operator()(
    const Entries &...entries) const {
    return {
      /*_entries=*/sort_alias_entries(
        alias_buffer<sizeof...(Entries)>{{entries...}}),
    };
  }
};

template <typename F, typename = void>
struct is_alias_resolver: std::false_type {};

template <typename F>
struct is_alias_resolver<F,
  compat::void_t<compat::invoke_result_t<F &,
    compat::string_view>>>:
    std::is_same<compat::optional<compat::string_view>,
      compat::invoke_result_t<F &, compat::string_view>> {};

} // namespace detail

/** Builds an alias-to-name resolver from tuple-like string-view groups.
 * Each group starts with the canonical name; remaining entries are aliases.
 * The caller must ensure aliases do not match another group's canonical
 * name or aliases; collisions are not validated. Mapping results are
 * unspecified for collisions. Canonical names are not implicitly added as
 * aliases. Returned views borrow the supplied strings, whose storage must
 * outlive use of the resolver.
 *
```text
aliases_dict :: (group, ...) -> (string_view -> optional<string_view>)
```
 *
 * Full constexpr factory evaluation requires C++14: std::make_tuple,
 * std::tuple_cat, and std::get are not guaranteed constexpr in C++11.
 * The sorting implementation itself supports C++11 constant evaluation.
 */
template <typename... Groups>
OMNI_CPP14_CONSTEXPR auto aliases_dict(const Groups &...groups)
  -> decltype(compat::apply(detail::cpp11_sorted_aliases{},
    std::tuple_cat(compat::apply(
      detail::cpp11_invert_alias_group{}, groups)...))) {
  return compat::apply(detail::cpp11_sorted_aliases{},
    std::tuple_cat(compat::apply(
      detail::cpp11_invert_alias_group{}, groups)...));
}

namespace detail {

// Overload ranking selects direct construction before adapting a pointer-only
// mapping. Its fallback alone requires a default-constructible destination.
template <typename Mapping, typename T, typename State>
auto map_node_to_value_using(State state,
  const Mapping &mapping,
  ::ryml::ConstNodeRef node,
  omni::type_t<T> type,
  int)
  -> decltype(mapping(std::move(state), node, type)) {
  return mapping(std::move(state), node, type);
}

OMNI_CONSTRAINED_TEMPLATE((typename Mapping, typename T, typename State),
  (std::is_default_constructible<T>::value))
auto map_node_to_value_using(State state,
  const Mapping &mapping,
  ::ryml::ConstNodeRef node,
  omni::type_t<T>,
  long)
  -> decltype(mapping(std::move(state), node, static_cast<T *>(nullptr)),
    with_diagnostics_t<T>{}) {
  auto value = compat::make_optional<T>();
  auto mapped = mapping(std::move(state), node, &*value);
  return {
    /*value=*/mapped.value ? std::move(value) : compat::nullopt,
    /*diagnostics=*/std::move(mapped.diagnostics),
  };
}

// Apply the same diagnostic limits to native and adapted value results,
// including custom mappings which return warnings with an available value.
template <typename Mapping, typename T, typename State>
auto map_node_to_value(State state,
  const Mapping &mapping,
  ::ryml::ConstNodeRef node,
  omni::type_t<T> type) -> with_diagnostics_t<T> {
  auto result = make_node_accumulator(state);
  if (result.stopped())
    return {compat::nullopt, std::move(result.result.diagnostics)};

  auto mapped = map_node_to_value_using(std::move(state),
    mapping,
    node,
    type,
    /*priority=*/0);

  result.append({
    /*value=*/mapped.value ? compat::make_optional(compat::monostate{})
      : compat::nullopt,
    /*diagnostics=*/std::move(mapped.diagnostics),
  });

  return {
    /*value=*/result.result.value ? std::move(mapped.value) : compat::nullopt,
    /*diagnostics=*/std::move(result.result.diagnostics),
  };
}

// C++11 overload ranking prefers value production when assignment is possible.
// The trailing decltype excludes mappings without the type-tag overload.
OMNI_CONSTRAINED_TEMPLATE((typename Mapping, typename T, typename State),
  (std::is_assignable<T &, T &&>::value))
auto read_into(State state,
  const Mapping &mapping,
  ::ryml::ConstNodeRef node,
  T *into,
  int)
  -> decltype(mapping(std::move(state), node, omni::type_t<T>{}),
    with_diagnostics_t<compat::monostate>{}) {
  return mapping(std::move(state), node, omni::type_t<T>{})
    .transform([into](T &&value) -> compat::monostate {
      *into = std::move(value);
      return {};
    });
}

template <typename Mapping, typename T, typename State>
auto read_into(State state,
  const Mapping &mapping,
  ::ryml::ConstNodeRef node,
  T *into,
  long) -> decltype(mapping(std::move(state), node, into)) {
  return mapping(std::move(state), node, into);
}

} // namespace detail
} // namespace ryml
} // namespace omni
