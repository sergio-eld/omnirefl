#pragma once

/** Pair construction and tuple access with C++11 constant evaluation.
 * The C++11 fallback supplies value initialization, forwarding and converting
 * constructors, and cv/ref-preserving get<I>. C++14 and later use std::pair.
 * @warning This header must remain independent of reflected_scope.hpp. */

#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

// C++14 requires constexpr forwarding construction for std::pair.
#if (defined(_MSVC_LANG) && 201402L <= _MSVC_LANG) \
  || (!defined(_MSVC_LANG) && 201402L <= __cplusplus)
namespace omni {
namespace compat {

using std::pair;
using std::get;
using std::make_pair;

} // namespace compat
} // namespace omni
#else
namespace omni {
namespace compat {
namespace detail {

template <typename First, typename Second>
struct cpp11_pair {
  using first_type = First;
  using second_type = Second;

  First first;
  Second second;

  template <typename A = First, typename B = Second,
    typename std::enable_if<std::is_default_constructible<A>::value
      && std::is_default_constructible<B>::value, int>::type = 0>
  constexpr cpp11_pair()
    noexcept(std::is_nothrow_default_constructible<A>::value
      && std::is_nothrow_default_constructible<B>::value)
    : first{}, second{} {}

  template <typename A, typename B,
    typename std::enable_if<std::is_constructible<First, A &&>::value
      && std::is_constructible<Second, B &&>::value, int>::type = 0>
  constexpr cpp11_pair(A &&a, B &&b)
    noexcept(std::is_nothrow_constructible<First, A &&>::value
      && std::is_nothrow_constructible<Second, B &&>::value)
    : first(std::forward<A>(a)), second(std::forward<B>(b)) {}

  template <typename A, typename B,
    typename std::enable_if<std::is_constructible<First, const A &>::value
      && std::is_constructible<Second, const B &>::value, int>::type = 0>
  constexpr cpp11_pair(const cpp11_pair<A, B> &other)
    noexcept(std::is_nothrow_constructible<First, const A &>::value
      && std::is_nothrow_constructible<Second, const B &>::value)
    : first(other.first), second(other.second) {}

  template <typename A, typename B,
    typename std::enable_if<std::is_constructible<First, A &&>::value
      && std::is_constructible<Second, B &&>::value, int>::type = 0>
  constexpr cpp11_pair(cpp11_pair<A, B> &&other)
    noexcept(std::is_nothrow_constructible<First, A &&>::value
      && std::is_nothrow_constructible<Second, B &&>::value)
    : first(std::forward<A>(other.first)),
      second(std::forward<B>(other.second)) {}

  // Preserve range construction of standard containers from enum entries.
  template <typename A, typename B,
    typename std::enable_if<std::is_constructible<A, const First &>::value
      && std::is_constructible<B, const Second &>::value, int>::type = 0>
  operator std::pair<A, B>() const & {
    return {first, second};
  }

  template <typename A, typename B,
    typename std::enable_if<std::is_constructible<A, First &&>::value
      && std::is_constructible<B, Second &&>::value, int>::type = 0>
  operator std::pair<A, B>() && {
    return {std::forward<First>(first), std::forward<Second>(second)};
  }
};

// An undefined primary keeps unrelated tuple arities out through SFINAE.
template <std::size_t Index>
struct cpp11_pair_element;

template <>
struct cpp11_pair_element<0> {
  template <typename Pair>
  static constexpr auto get(Pair &&value) noexcept
    -> decltype((std::forward<Pair>(value).first)) {
    return std::forward<Pair>(value).first;
  }
};

template <>
struct cpp11_pair_element<1> {
  template <typename Pair>
  static constexpr auto get(Pair &&value) noexcept
    -> decltype((std::forward<Pair>(value).second)) {
    return std::forward<Pair>(value).second;
  }
};

template <std::size_t Index, typename First, typename Second>
constexpr auto get(cpp11_pair<First, Second> &value) noexcept
  -> decltype(cpp11_pair_element<Index>::get(value)) {
  return cpp11_pair_element<Index>::get(value);
}

template <std::size_t Index, typename First, typename Second>
constexpr auto get(const cpp11_pair<First, Second> &value) noexcept
  -> decltype(cpp11_pair_element<Index>::get(value)) {
  return cpp11_pair_element<Index>::get(value);
}

template <std::size_t Index, typename First, typename Second>
constexpr auto get(cpp11_pair<First, Second> &&value) noexcept
  -> decltype(cpp11_pair_element<Index>::get(std::move(value))) {
  return cpp11_pair_element<Index>::get(std::move(value));
}

template <std::size_t Index, typename First, typename Second>
constexpr auto get(const cpp11_pair<First, Second> &&value) noexcept
  -> decltype(cpp11_pair_element<Index>::get(std::move(value))) {
  return cpp11_pair_element<Index>::get(std::move(value));
}

template <typename T>
struct cpp11_unwrap_reference { using type = T; };

template <typename T>
struct cpp11_unwrap_reference<std::reference_wrapper<T>> { using type = T &; };

template <typename T>
using cpp11_pair_value_t =
  typename cpp11_unwrap_reference<typename std::decay<T>::type>::type;

} // namespace detail

template <typename First, typename Second>
using pair = detail::cpp11_pair<First, Second>;

/** Pair construction with decayed values and unwrapped reference_wrapper
 * arguments, including C++11 constant evaluation. */
template <typename First, typename Second>
constexpr pair<detail::cpp11_pair_value_t<First>,
  detail::cpp11_pair_value_t<Second>>
make_pair(First &&first, Second &&second) {
  return {std::forward<First>(first), std::forward<Second>(second)};
}

using std::get;
using detail::get;

} // namespace compat
} // namespace omni

// libstdc++ 7 declares these templates with both class and struct. Clang 6
// diagnoses either spelling in our specializations, although the tags are
// interchangeable here. Limit the compatibility workaround to these traits.
#if defined(__clang__) && __clang_major__ < 7 \
  && defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE <= 7
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wmismatched-tags"
#endif

namespace std {

template <typename First, typename Second>
struct tuple_size<omni::compat::detail::cpp11_pair<First, Second>>:
    integral_constant<size_t, 2> {};

template <size_t Index, typename First, typename Second>
struct tuple_element<Index, omni::compat::detail::cpp11_pair<First, Second>> {
  static_assert(Index < 2, "pair index must be 0 or 1");
  using type = typename conditional<Index == 0, First, Second>::type;
};

} // namespace std

#if defined(__clang__) && __clang_major__ < 7 \
  && defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE <= 7
#  pragma clang diagnostic pop
#endif
#endif
