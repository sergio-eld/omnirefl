#pragma once

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <tl/expected.hpp>

#include <filesystem>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace util {

struct convert_t {
  template <typename C,
    typename From,
    template <typename>
    class Alloc,
    typename To = std::invoke_result_t<C, From>>
  auto operator()(C &&convert, std::vector<From, Alloc<From>> &&from) const noexcept
    -> std::vector<To, Alloc<To>> {
    std::vector<std::invoke_result_t<C, From>> result;
    result.reserve(from.size());
    for (auto &&f : from)
      result.emplace_back(convert(std::move(f)));
    return result;
  }

  template <typename C,
    typename From,
    template <typename>
    class Alloc,
    typename To = std::invoke_result_t<C, From>>
  auto operator()(C &&convert, const std::vector<From, Alloc<From>> &from) const noexcept
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
constexpr auto indexed(Container &c) noexcept {
  using _iter_type = decltype(std::begin(c));
  using value_type = std::decay_t<decltype(*std::begin(c))>;
  struct _ref {
    std::size_t index;
    const value_type &value;
  };
  struct _indexed_iter {
    _iter_type _begin;
    _iter_type _end;
    _iter_type _iter = _begin;
    std::size_t _index = 0;

    constexpr _indexed_iter begin() const {
      return {_begin, _end};
    }
    constexpr _indexed_iter end() const {
      return {_begin, _end, _end};
    }
    constexpr _indexed_iter operator++() {
      ++_iter;
      ++_index;
      return *this;
    }
    constexpr bool operator==(const _indexed_iter &other) const noexcept {
      return _iter == other._iter;
    }
    constexpr bool operator!=(const _indexed_iter &other) const noexcept {
      return _iter != other._iter;
    }
    constexpr _ref operator*() const {
      return {_index, *_iter};
    }
  };
  return _indexed_iter{std::begin(c), std::end(c)};
}

template <typename T, typename Format>
struct with_fmt {
  const T &value;
  Format format;
};

template <typename T>
struct _with_fmt_rng {
  using _wrapped_value_type = decltype(*std::declval<const T &>().begin());
  using _wrapped_iterator_type = decltype(std::declval<const T &>().begin());

  constexpr static auto begin(const T &t) noexcept {
    return std::begin(t);
  }
  constexpr static auto end(const T &t) noexcept {
    return std::end(t);
  }
};

template <typename T>
struct _with_fmt_rng<std::reference_wrapper<T>> {
  using _wrapped_value_type = decltype(*std::declval<const T &>().begin());
  using _wrapped_iterator_type = decltype(std::declval<const T &>().begin());

  constexpr static auto begin(std::reference_wrapper<T> t) noexcept {
    return std::begin(t.get());
  }
  constexpr static auto end(std::reference_wrapper<T> t) noexcept {
    return std::end(t.get());
  }
};

// refactorme: pretty cumbersome interface
template <typename T, typename Format>
struct with_fmt_rng {
  T value;
  Format format;

  // fixme: should not force `const` in `std::reference_wrapper<Format>`, but otherwise doesn't
  // compile..
  using value_type =
    with_fmt<typename _with_fmt_rng<T>::_wrapped_value_type, std::reference_wrapper<const Format>>;
  using _wrapped_iterator_type = typename _with_fmt_rng<T>::_wrapped_iterator_type;

  struct iterator_type {
    using value_type = with_fmt_rng::value_type;

    _wrapped_iterator_type i;
    std::reference_wrapper<const Format> format;
    value_type operator*() const {
      return {.value = *i, .format = format};
    }
    _wrapped_iterator_type operator++() {
      return ++i;
    }
    bool operator==(const iterator_type &other) const {
      return i == other.i;
    }
    bool operator!=(const iterator_type &other) const {
      return i != other.i;
    }
  };

  constexpr iterator_type begin() const {
    return iterator_type{.i = _with_fmt_rng<T>::begin(value), .format = std::ref(format)};
  }
  constexpr iterator_type end() const {
    return iterator_type{.i = _with_fmt_rng<T>::end(value), .format = std::ref(format)};
  }
};

template <typename T, typename Format>
with_fmt(const T &, Format) -> with_fmt<T, Format>;

// todo: error for non-path strings
auto to_std_paths(const auto &strings)
  -> tl::expected<std::vector<std::filesystem::path>, std::string> {
  tl::expected<std::vector<std::filesystem::path>, std::string> result{tl::in_place};
  result->reserve(strings.size());
  std::error_code ec{};
  for (const auto &s : strings) {
    result->push_back(std::filesystem::absolute(s, ec).lexically_normal());
    if (ec)
      return tl::unexpected("Invalid path `" + s + "`: " + ec.message());
  }
  return result;
}

inline bool is_subpath(const std::filesystem::path &path, const std::filesystem::path &base) {
  const auto mismatch_pair = std::mismatch(path.begin(), path.end(), base.begin(), base.end());
  return mismatch_pair.second == base.end();
};

struct foldl_t {
  template <typename Callable, typename Result, typename Container>
  constexpr auto operator()(Callable &&op, Result &&initial, Container &&container) const
    -> std::decay_t<Result> {
    auto _op = std::forward<Callable>(op);
    auto _res = std::forward<Result>(initial);
    for (auto &&e : std::forward<Container>(container)) {
      _res = _op(std::move(_res), std::forward<decltype(e)>(e));
    }
    return _res;
  }

  template <typename Callable, typename Result>
  constexpr auto operator()(Callable &&op, Result &&initial) const noexcept {
    return [op = std::forward<Callable>(op), initial = std::forward<Result>(initial)](
             auto &&container) mutable -> decltype(auto) {
      return foldl_t{}(std::forward<Callable>(op),
        std::forward<Result>(initial),
        std::forward<decltype(container)>(container));
    };
  }
} constexpr inline foldl{};

template <typename>
struct to_tuple;

template <typename ... T, template <typename...> class List>
struct to_tuple<List<T...>> { using type = std::tuple<T...>; };

template <typename List>
using to_tuple_t = typename to_tuple<List>::type;

} // namespace util

// the only way is to specialize in the global namespace
template <typename T, typename Format, typename Char>
struct fmt::formatter<util::with_fmt<T, Format>, Char> {
  constexpr auto parse(fmt::format_parse_context &ctx) {
    return ctx.begin();
  }
  constexpr static auto format(const util::with_fmt<T, Format> &uf, fmt::format_context &ctx) {
    return uf.format(uf.value, ctx);
  }
};
