#pragma once

#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

// C++ version (MSVC uses _MSVC_LANG)
#if defined(_MSC_VER) && defined(_MSVC_LANG)
#  define OMNI_CPLUSPLUS _MSVC_LANG
#else
#  define OMNI_CPLUSPLUS __cplusplus
#endif

namespace omni {
namespace compat {

template <typename T>
struct type_identity {
  using type = T;
};

template <std::size_t...>
struct index_sequence {};

namespace detail {

// generate index_sequence<0,1,2,...,N-1>
template <class T, T I, T N, T... integers>
struct _make_integer_sequence {
  using type =
    typename _make_integer_sequence<T, I + 1, N, integers..., I>::type;
};

template <class T, T N, T... integers>
struct _make_integer_sequence<T, N, N, integers...> {
  using type = index_sequence<integers...>;
};

template <class T, T N>
using make_integer_sequence = typename _make_integer_sequence<T, 0, N>::type;

template <typename Visit, typename Tuple, std::size_t... I>
constexpr auto _apply(Visit &&v, Tuple &&t, index_sequence<I...>)
  -> decltype(std::forward<Visit>(v)(std::get<I>(std::forward<Tuple>(t))...)) {
  return std::forward<Visit>(v)(std::get<I>(std::forward<Tuple>(t))...);
}

template <typename Visit, typename Tuple>
constexpr auto apply(Visit &&v, Tuple &&t)
  -> decltype(_apply(std::forward<Visit>(v),
    std::forward<Tuple>(t),
    make_integer_sequence<std::size_t,
      std::tuple_size<typename std::decay<Tuple>::type>::value>{})) {
  return _apply(std::forward<Visit>(v),
    std::forward<Tuple>(t),
    make_integer_sequence<std::size_t,
      std::tuple_size<typename std::decay<Tuple>::type>::value>{});
}

template <typename... Ts>
struct make_void {
  using type = void;
};

} // namespace detail

// make_index_sequence
#if OMNI_CPLUSPLUS >= 201402L
template <std::size_t N>
using make_index_sequence = std::make_index_sequence<N>;
#else
template <std::size_t N>
using make_index_sequence =
  typename detail::_make_integer_sequence<std::size_t, 0, N>::type;
#endif

// void_t
#if OMNI_CPLUSPLUS >= 201703L
using std::void_t;
#else
template <typename... Ts>
using void_t = typename detail::make_void<Ts...>::type;
#endif

// apply
#if OMNI_CPLUSPLUS >= 201703L
using std::apply;
#else
using detail::apply;
#endif

// conditional_t
#if OMNI_CPLUSPLUS >= 201402L
using std::conditional_t;
#else
template <bool B, class T, class F>
using conditional_t = typename std::conditional<B, T, F>::type;
#endif

// decay_t
#if OMNI_CPLUSPLUS >= 201402L
using std::decay_t;
#else
template <typename T>
using decay_t = typename std::decay<T>::type;
#endif

// disjunction
#if OMNI_CPLUSPLUS >= 201703L
using std::disjunction;
#else
template <class...>
struct disjunction: std::false_type {};

template <class B1>
struct disjunction<B1>: B1 {};

template <class B1, class... Bn>
struct disjunction<B1, Bn...>:
    std::conditional<bool(B1::value), B1, disjunction<Bn...>>::type {};
#endif

// remove_cvref / remove_cvref_t
#if OMNI_CPLUSPLUS >= 202002L
using std::remove_cvref;
using std::remove_cvref_t;
#else
template <class T>
struct remove_cvref {
  using type =
    typename std::remove_cv<typename std::remove_reference<T>::type>::type;
};

template <class T>
using remove_cvref_t = typename remove_cvref<T>::type;
#endif

inline const std::string &to_string(const std::string &v) noexcept {
  return v;
}

inline std::string to_string(std::string &&v) noexcept {
  return std::move(v);
}

template <typename T>
auto to_string(const T &v) -> decltype(std::to_string(v)) {
  return std::to_string(v);
}

} // namespace compat
} // namespace omni
