#pragma once

#include <omnirefl/extensions/compat.hpp>

#include <algorithm>
#include <array>
#include <tuple>
#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {

template <bool OwnsTree>
struct diagnostics;

template <typename T, typename Diagnostics>
struct with_diagnostics;

namespace detail {

struct mapping_context;

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

// Views borrow the strings supplied to alias_dict; keep their storage alive.
template <std::size_t Size>
struct name_by_alias_lookup {
  // alias_dict sorts these entries; users must not modify them afterward.
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
struct is_alias_by_name: std::false_type {};

template <typename F>
struct is_alias_by_name<F,
  compat::void_t<compat::invoke_result_t<F &,
    compat::string_view>>>:
    std::is_same<compat::optional<compat::string_view>,
      compat::invoke_result_t<F &, compat::string_view>> {};

} // namespace detail

/** Build an alias-to-name resolver from tuple-like string-view groups.
 * Each group starts with the canonical name; remaining entries are aliases.
 * The caller must ensure aliases do not match another group's canonical
 * name or aliases; collisions are not validated. Mapping results are
 * unspecified for collisions. Canonical names are not implicitly added as
 * aliases. Returned views borrow the supplied strings, whose storage must
 * outlive use of the resolver.
 * Full constexpr factory evaluation requires C++14: std::make_tuple,
 * std::tuple_cat, and std::get are not guaranteed constexpr in C++11.
 * The sorting implementation itself supports C++11 constant evaluation.
 */
template <typename... Groups>
OMNI_CPP14_CONSTEXPR auto alias_dict(const Groups &...groups)
  -> decltype(compat::apply(detail::cpp11_sorted_aliases{},
    std::tuple_cat(compat::apply(
      detail::cpp11_invert_alias_group{}, groups)...))) {
  return compat::apply(detail::cpp11_sorted_aliases{},
    std::tuple_cat(compat::apply(
      detail::cpp11_invert_alias_group{}, groups)...));
}

// TODO: [medium] Add QoL validation against other aliases and reflected
// canonical names, possibly usable inline when constructing a dictionary.

} // namespace ryml
} // namespace omni
