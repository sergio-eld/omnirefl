#pragma once

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <tl/expected.hpp>

#include <filesystem>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace util {

struct convert_t {
  template <typename C,
    typename From,
    template <typename> class Alloc,
    typename To = std::invoke_result_t<C, From>>
  auto operator()(C &&convert,
    std::vector<From, Alloc<From>> &&from) const noexcept
    -> std::vector<To, Alloc<To>> {
    std::vector<std::invoke_result_t<C, From>> result;
    result.reserve(from.size());
    for (auto &&f : from)
      result.emplace_back(convert(std::move(f)));
    return result;
  }

  template <typename C,
    typename From,
    template <typename> class Alloc,
    typename To = std::invoke_result_t<C, From>>
  auto operator()(C &&convert,
    const std::vector<From, Alloc<From>> &from) const noexcept
    -> std::vector<To, Alloc<To>> {
    std::vector<std::invoke_result_t<C, From>> result;
    result.reserve(from.size());
    for (const auto &f : from)
      result.emplace_back(convert(f));
    return result;
  }
} constexpr inline converted{};

namespace str {
struct is_empty_t {
  constexpr bool operator()(std::string_view s) const {
    return s.empty();
  }
} constexpr const inline is_empty{};
} // namespace str

struct sorted_t {
  template <typename Cmp, typename Container>
  Container operator()(Cmp cmp, Container &&c) const {
    // todo: what if non-const reference?
    Container _s = std::forward<Container>(c);
    std::sort(_s.begin(), _s.end(), cmp);
    return _s;
  }
} constexpr const inline sorted{};

struct filtered_t {
  template <typename Condition, typename Container>
  auto operator()(Condition cnd, Container &&c) const {
    // todo: what if non-const reference?
    Container _c = std::forward<Container>(c);
    std::erase_if(_c, cnd);
    return _c;
  }
} constexpr const inline filtered{};

template <typename Container>
constexpr auto indexed(Container &&c) noexcept {
  using iterator_type = decltype(std::begin(c));
  using reference_type = decltype(*std::begin(c));
  struct _ref {
    reference_type value;
    std::size_t index;
  };
  struct _indexed_iterator {
    iterator_type _begin;
    iterator_type _end;
    iterator_type _iter = _begin;
    std::size_t _index = 0;

    constexpr _indexed_iterator begin() const {
      return {_begin, _end};
    }
    constexpr _indexed_iterator end() const {
      return {_begin, _end, _end};
    }
    constexpr _indexed_iterator &operator++() {
      ++_iter;
      ++_index;
      return *this;
    }
    constexpr bool operator==(const _indexed_iterator &other) const noexcept {
      return _iter == other._iter;
    }
    constexpr bool operator!=(const _indexed_iterator &other) const noexcept {
      return _iter != other._iter;
    }
    constexpr _ref operator*() const {
      return {*_iter, _index};
    }
  };
  return _indexed_iterator{std::begin(c), std::end(c)};
}

template <typename T, typename Format, typename = void>
struct _wrapped_element {
  using value_type = std::conditional_t< //
    std::is_lvalue_reference_v<T>,
    std::add_const_t<std::remove_reference_t<T>> &,
    T>;
  value_type value;
  Format format;
};

template <typename Iter, typename Format>
struct _wrapped_iterator {
  using value_type =
    _wrapped_element<std::decay_t<decltype(*std::declval<Iter>())>, Format>;

  Iter i;
  Format format;
  constexpr value_type operator*() const {
    return {.value = *i, .format = format};
  }

  constexpr _wrapped_iterator &operator++() {
    ++i;
    return *this;
  }
  constexpr bool operator==(const _wrapped_iterator &other) const {
    return i == other.i;
  }
  constexpr bool operator!=(const _wrapped_iterator &other) const {
    return i != other.i;
  }
};

template <typename FormatString, typename Element>
constexpr auto _format(std::false_type,
  FormatString fmt_string,
  const Element &e,
  fmt::format_context &ctx) {
  return fmt::format_to(ctx.out(), fmt_string(), e);
}

template <typename FormatString, typename Element>
constexpr auto _format(std::true_type,
  FormatString fmt_string,
  const Element &e,
  fmt::format_context &ctx) {
  return fmt_string(e, ctx);
}

template <typename Range, typename Char, typename FmtStr>
constexpr auto join(const Range &rng,
  std::basic_string_view<Char> delim,
  FmtStr fmt_string) {
  const auto formatter = [fmt_string](const auto &e, fmt::format_context &ctx) {
    return _format(std::is_invocable<FmtStr, decltype(e), decltype(ctx)>{},
      fmt_string,
      e,
      ctx);
  };
  return fmt::join(_wrapped_iterator{rng.begin(), formatter},
    _wrapped_iterator{rng.end(), formatter},
    delim);
}

template <typename Range, typename Char, size_t N, typename FmtStr>
constexpr auto
  join(const Range &rng, const Char (&delim)[N], FmtStr fmt_string) {
  return join(rng, std::basic_string_view<Char>(delim), fmt_string);
}

// todo: error for non-path strings
auto to_std_paths(const auto &strings)
  -> tl::expected<std::vector<std::filesystem::path>, std::string> {
  tl::expected<std::vector<std::filesystem::path>, std::string> result{
    tl::in_place};
  result->reserve(strings.size());
  std::error_code ec{};
  for (const auto &s : strings) {
    result->push_back(std::filesystem::absolute(s, ec).lexically_normal());
    if (ec)
      return tl::unexpected("Invalid path `" + s + "`: " + ec.message());
  }
  return result;
}

inline bool is_subpath(const std::filesystem::path &path,
  const std::filesystem::path &base) {
  const auto mismatch_pair =
    std::mismatch(path.begin(), path.end(), base.begin(), base.end());
  return mismatch_pair.second == base.end();
}

struct foldl_t {
  template <typename Callable, typename Result, typename Container>
  constexpr auto operator()(Callable &&op,
    Result &&initial,
    Container &&container) const -> std::decay_t<Result> {
    auto _op = std::forward<Callable>(op);
    auto _res = std::forward<Result>(initial);
    for (auto &&elem : std::forward<Container>(container)) {
      _res = _op(std::move(_res), std::forward<decltype(elem)>(elem));
    }
    return _res;
  }

  template <typename Callable, typename Result>
  constexpr auto operator()(Callable &&op, Result &&initial) const noexcept {
    return [op = std::forward<Callable>(op),
             initial = std::forward<Result>(initial)](
             auto &&container) mutable -> decltype(auto) {
      return foldl_t{}(std::forward<Callable>(op),
        std::forward<Result>(initial),
        std::forward<decltype(container)>(container));
    };
  }
} constexpr inline foldl{};

template <typename>
struct to_tuple;

template <typename... T, template <typename...> class List>
struct to_tuple<List<T...>> {
  using type = std::tuple<T...>;
};

template <typename List>
using to_tuple_t = typename to_tuple<List>::type;

// refactorme: better interface (cmp is ambiguous)
template <typename K, typename T, typename Cmp>
tl::expected<std::unordered_map<K, T>, std::string> merge_with_conflicts_check(
  std::unordered_map<K, T> first,
  std::unordered_map<K, T> second,
  Cmp cmp) {
  for (auto node = second.begin(); node != second.end();) {
    auto &&[k, v] = *node;
    const auto found = first.find(k);
    if (found == first.cend()) {
      first.insert(second.extract(node++));
      continue;
    }
    if (tl::expected res = cmp(k, v, found->second); !res)
      return tl::unexpected(std::move(res).error());
    ++node;
  }
  return {std::move(first)};
}

} // namespace util

// the only way is to specialize in the global namespace
template <typename T, typename Format, typename Char>
struct fmt::formatter<util::_wrapped_element<T, Format>, Char> {
  constexpr auto parse(fmt::format_parse_context &ctx) {
    return ctx.begin();
  }
  constexpr auto format(const util::_wrapped_element<T, Format> &we,
    fmt::format_context &ctx) const {
    return we.format(we.value, ctx);
  }
};
