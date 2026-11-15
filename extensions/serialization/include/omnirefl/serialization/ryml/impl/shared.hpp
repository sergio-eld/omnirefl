#pragma once

// TODO: [medium] Validate alias_dict against other aliases and reflected
// canonical names, possibly inline when constructing a dictionary.

#include <omnirefl/serialization/ryml/state.hpp>
#include <omnirefl/extensions/traits.hpp>

#include <c4/format.hpp>
#include <c4/std/string.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <tuple>
#include <type_traits>
#include <utility>

// c4 views use the same operation customization points as user strings.
template <>
struct omni::traits::cp::as_string<c4::csubstr> {
  constexpr omni::compat::string_view operator()(c4::csubstr value) const {
    return {value.str, value.len};
  }
};

template <>
struct omni::traits::cp::assign_string<c4::csubstr> {
  void operator()(c4::csubstr *into, omni::compat::string_view text) const {
    *into = {text.data(), text.size()};
  }
};

namespace omni {
namespace ryml {
namespace core {

// Mapper bodies delegate recursively through these dispatchers, whose
// definitions live in the umbrella's implementation header. The first
// declaration supplies SFINAE defaults; later declarations omit them.
OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
  (is_read_state<State>::value))
auto read_node(::ryml::ConstNodeRef node, T *into, State state)
  -> with_diagnostics_t<compat::monostate, diagnostics>;

OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
  (is_write_state<State>::value))
void write_node(const T &value, ::ryml::NodeRef node, State state);

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
struct is_alias_resolver: std::false_type {};

template <typename F>
struct is_alias_resolver<F,
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

namespace detail {

constexpr compat::string_view to_string_view(c4::csubstr s) {
  // Keep empty views safe to append through C++11 string's pointer/size API.
  return {s.len ? s.str : "", s.len};
}

inline json::value_kind node_kind(::ryml::ConstNodeRef n) {
  if (n.invalid())
    return json::value_kind::unknown;

  if (n.is_map())
    return json::value_kind::mapping;

  if (n.is_seq())
    return json::value_kind::sequence;

  if (n.has_val())
    return n.val_is_null() ? json::value_kind::null : json::value_kind::scalar;

  return json::value_kind::unknown;
}

inline std::string render_path(
  const std::vector<issue::path_segment> &segments) {
  std::string path;
  for (const auto &segment : segments) {
    path += '/';
    if (segment.index) {
      path += std::to_string(*segment.index);
      continue;
    }

    for (const char ch : segment.field) {
      if ('~' == ch)
        path += "~0";
      else if ('/' == ch)
        path += "~1";
      else
        path += ch;
    }
  }

  return path;
}

OMNI_CPP14_CONSTEXPR inline compat::string_view kind_name(json::value_kind k) {
  switch (k) {
  case json::value_kind::mapping:
    return "object";
  case json::value_kind::sequence:
    return "array";
  case json::value_kind::string:
    return "string";
  case json::value_kind::boolean:
    return "boolean";
  case json::value_kind::integer:
    return "integer";
  case json::value_kind::unsigned_integer:
    return "unsigned integer";
  case json::value_kind::number:
    return "real number";
  case json::value_kind::null:
    return "null";
  default:
    return "scalar";
  }
}

// Local traversal output owns the accumulated diagnostics and limit counters.
// Child states snapshot progress; diagnostics are returned separately.
template <typename State>
struct node_accumulator {
  State state;
  with_diagnostics_t<compat::monostate, diagnostics> result;

  bool stopped() const { return this->state.limit_exceeded(); }

  void report(issue entry) {
    this->state.error_count += !entry.warning;
    this->state.warning_count += entry.warning;
    if (!entry.warning)
      this->result.value.reset();

    this->result.diagnostics.issues.push_back(std::move(entry));
    if (this->result.diagnostics.issues.back().warning
      && this->state.config.error_policy.warning_limit
      && this->state.warning_count
        == std::size_t{1} + *this->state.config.error_policy.warning_limit) {
      auto exceeded = this->result.diagnostics.issues.back();
      exceeded.reason = json::issue_code::warning_limit_exceeded;
      exceeded.warning = false;
      this->result.diagnostics.issues.push_back(std::move(exceeded));
      ++this->state.error_count;
      this->result.value.reset();
    }
  }

  void append(with_diagnostics_t<compat::monostate, diagnostics> child) {
    if (!child.value)
      this->result.value.reset();

    // A child may already have emitted the limit error. Preserve that report
    // without generating the same warning-limit error again at each parent.
    const bool has_limit_error = std::any_of(child.diagnostics.issues.begin(),
      child.diagnostics.issues.end(),
      [](const issue &entry) {
        return entry.reason == json::issue_code::warning_limit_exceeded;
      });

    for (auto &entry : child.diagnostics.issues) {
      if (has_limit_error) {
        this->state.error_count += !entry.warning;
        this->state.warning_count += entry.warning;
        if (!entry.warning)
          this->result.value.reset();

        this->result.diagnostics.issues.push_back(std::move(entry));
      } else {
        this->report(std::move(entry));
      }
    }
    if (this->stopped())
      this->result.value.reset();
  }

  auto finish() && -> with_diagnostics_t<compat::monostate, diagnostics> {
    return std::move(this->result);
  }
};

OMNI_CONSTRAINED_TEMPLATE((typename State), (is_read_state<State>::value))
node_accumulator<State> make_node_accumulator(State state) {
  return {
    /*state=*/state,
    /*result=*/{
      /*value=*/compat::monostate{},
      /*diagnostics=*/{
        /*config=*/{state.config.error_policy, {}},
        /*issues=*/{},
        /*parse_error_message=*/{},
      },
    },
  };
}

inline issue make_issue(json::issue_code reason,
  json::value_kind expected,
  ::ryml::ConstNodeRef node,
  mapping_path path,
  bool warning = false) {
  // C++11 cannot aggregate-initialize issue because it has member defaults.
  issue result{};
  result.reason = reason;
  result.path = path.materialize();
  result.expected = expected;
  result.actual = node_kind(node);
  result.warning = warning;

  if (!node.invalid())
    result.node_id = node.id();

  return result;
}

// Format validation precedes conversion; failure here means out of range.
OMNI_CONSTRAINED_TEMPLATE((typename T), (std::is_integral<T>::value))
compat::expected<T, json::issue_code> from_chars(c4::csubstr scalar) {
  T value{};
  // C4 accepts a pointer to either the checked wrapper or the original value.
  compat::conditional_t<std::is_same<bool, T>::value,
    T &,
    c4::fmt::overflow_checked_<T>> target{value};

  if (!c4::from_chars(scalar, &target))
    return compat::unexpected<json::issue_code>{json::issue_code::out_of_range};

  return value;
}

OMNI_CONSTRAINED_TEMPLATE((typename T), (!std::is_integral<T>::value))
compat::expected<T, json::issue_code> from_chars(c4::csubstr scalar) {
  T value{};
  if (!c4::from_chars(scalar, &value))
    return compat::unexpected<json::issue_code>{json::issue_code::out_of_range};

  return value;
}

} // namespace detail

} // namespace ryml
} // namespace omni
