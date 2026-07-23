#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <version>

// Standard-library compatibility names used primarily for Cosmopolitan,
// whose bundled C++ library does not yet provide every required C++23 facility.
namespace std_c {

namespace detail {

struct enumerate_impl_t {
  template <std::ranges::viewable_range R>
    requires std::ranges::random_access_range<std::views::all_t<R>>
      && std::ranges::sized_range<std::views::all_t<R>>
  constexpr auto operator()(R &&range) const {
    auto view = std::views::all(std::forward<R>(range));
    const auto indexes =
      std::views::iota(std::size_t{}, std::ranges::size(view));

    return indexes
      | std::views::transform(
        [view = std::move(view)](const std::size_t index) {
          using reference = std::ranges::range_reference_t<decltype(view)>;
          return std::tuple<std::size_t, reference>{index, view[index]};
        });
  }

  template <std::ranges::viewable_range R>
    requires std::ranges::random_access_range<std::views::all_t<R>>
      && std::ranges::sized_range<std::views::all_t<R>>
  friend constexpr auto operator|(R &&range, enumerate_impl_t self) {
    return self(std::forward<R>(range));
  }
};

inline constexpr enumerate_impl_t enumerate_impl{};

struct join_with_adaptor {
  std::string_view separator;

  template <std::ranges::input_range R>
  friend std::string operator|(R &&range, join_with_adaptor self) {
    std::string result;
    bool first = true;

    for (auto &&value : range) {
      if (!first)
        result += self.separator;

      std::ranges::copy(value, std::back_inserter(result));
      first = false;
    }

    return result;
  }
};

struct join_with_impl_t {
  constexpr join_with_adaptor operator()(std::string_view separator) const {
    return {.separator = separator};
  }
};

inline constexpr join_with_impl_t join_with_impl{};

} // namespace detail

namespace views {

#if defined(__cpp_lib_ranges_enumerate) \
  && 202302L <= __cpp_lib_ranges_enumerate
using std::views::enumerate;
#else
inline constexpr auto enumerate = detail::enumerate_impl;
#endif

#if defined(__cpp_lib_ranges_join_with) \
  && 202202L <= __cpp_lib_ranges_join_with
using std::views::join_with;
#else
inline constexpr auto join_with = detail::join_with_impl;
#endif

} // namespace views

} // namespace std_c
