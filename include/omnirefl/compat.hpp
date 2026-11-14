#pragma once

// Standard-library compatibility for the C++11 public baseline. Facilities
// use their standard implementation when available and a local drop-in
// otherwise.

#include <functional>
#include <iterator>
#include <memory>
// FIXME: [low] Freestanding: separate the owning-string/to_string helpers and
// this hosted dependency from the common compatibility header.
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

// C++20 ranges are optional on standard libraries predating ranges support.
#if 202002L <= OMNI_CPLUSPLUS && defined(__has_include)
#  if __has_include(<ranges>)
#    include <ranges>
#  endif
#endif

// Enabled in C++14 and newer for functions whose bodies cannot be constexpr
// under C++11's single-return-statement restriction.
#if 201402L <= OMNI_CPLUSPLUS
#  define OMNI_CPP14_CONSTEXPR constexpr
#else
#  define OMNI_CPP14_CONSTEXPR
#endif

// C++14 aggregates allow member defaults; omit them to preserve C++11 aggregates.
#if defined(__cpp_aggregate_nsdmi) && 201304L <= __cpp_aggregate_nsdmi
#  define OMNI_CPP14_DEFAULT_INIT(...) = {__VA_ARGS__}
#else
#  define OMNI_CPP14_DEFAULT_INIT(...)
#endif

// C++17 constexpr support includes literal lambda closure types.
#if defined(__cpp_constexpr) && 201603L <= __cpp_constexpr
#  define OMNI_CPP17_CONSTEXPR constexpr
#else
#  define OMNI_CPP17_CONSTEXPR
#endif

// Supply a fallback return type when C++14 return-type deduction is unavailable.
// Variadic arguments also accept types containing template-argument commas.
#if !defined(__cpp_return_type_deduction) \
  || 201304L > __cpp_return_type_deduction
#  define OMNI_CPP11_TRAILING_RETURN(...) -> __VA_ARGS__
#else
#  define OMNI_CPP11_TRAILING_RETURN(...)
#endif

#define OMNI_DETAIL_UNPAREN(...) __VA_ARGS__

/** (QoL) Declare a constrained template across supported C++ standards.
 * Parameters is a parenthesized template-parameter list; Constraint is a
 * parenthesized boolean expression. Expands to a template declaration with
 * a requires-clause in C++20, or a defaulted SFINAE parameter earlier.
 *
```cpp
OMNI_CONSTRAINED_TEMPLATE(
  (typename F, typename T),
  (omni::compat::is_invocable<F &, T>::value))
auto apply(F f, T value) -> decltype(f(value));

// C++20 expansion:
template <typename F, typename T>
  requires (omni::compat::is_invocable<F &, T>::value)
auto apply(F f, T value) -> decltype(f(value));

// C++11/14/17 expansion:
template <typename F, typename T,
  typename std::enable_if<
    omni::compat::is_invocable<F &, T>::value, int>::type = 0>
auto apply(F f, T value) -> decltype(f(value));
```
 * @details One spelling avoids per-declaration compatibility guards.
 * Parentheses protect commas inside both arguments. Uses std::enable_if
 * so it also works before compat::enable_if_t is declared in this header.
 */
#if defined(__cpp_concepts) && 201907L <= __cpp_concepts
#  define OMNI_CONSTRAINED_TEMPLATE(Parameters, Constraint) \
    template <OMNI_DETAIL_UNPAREN Parameters> \
      requires (OMNI_DETAIL_UNPAREN Constraint)
#else
#  define OMNI_CONSTRAINED_TEMPLATE(Parameters, Constraint) \
    template <OMNI_DETAIL_UNPAREN Parameters, \
      typename ::std::enable_if<OMNI_DETAIL_UNPAREN Constraint, \
        int>::type = 0>
#endif

// Invoke beside the enum so argument-dependent lookup finds these operators.
#define OMNI_ENABLE_BIT_FLAGS(Enum)                                        \
  constexpr Enum operator|(Enum left, Enum right) noexcept {               \
    return static_cast<Enum>(                                              \
      static_cast<typename std::underlying_type<Enum>::type>(left)         \
      | static_cast<typename std::underlying_type<Enum>::type>(right));    \
  }                                                                        \
  constexpr Enum operator&(Enum left, Enum right) noexcept {               \
    return static_cast<Enum>(                                              \
      static_cast<typename std::underlying_type<Enum>::type>(left)         \
      & static_cast<typename std::underlying_type<Enum>::type>(right));    \
  }                                                                        \
  constexpr Enum operator^(Enum left, Enum right) noexcept {               \
    return static_cast<Enum>(                                              \
      static_cast<typename std::underlying_type<Enum>::type>(left)         \
      ^ static_cast<typename std::underlying_type<Enum>::type>(right));    \
  }                                                                        \
  constexpr Enum operator~(Enum value) noexcept {                          \
    return static_cast<Enum>(                                              \
      ~static_cast<typename std::underlying_type<Enum>::type>(value));     \
  }

namespace omni {
namespace compat {

template <typename T>
struct type_identity {
  using type = T;
};

// decay_t
#if OMNI_CPLUSPLUS >= 201402L
using std::decay_t;
#else
template <typename T>
using decay_t = typename std::decay<T>::type;
#endif

#if OMNI_CPLUSPLUS >= 201402L
using std::index_sequence;
#else
template <std::size_t...>
struct index_sequence {};
#endif

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

template <std::size_t I, typename Tuple>
using tuple_element_rvalue_t = typename std::add_rvalue_reference<
  typename std::tuple_element<
    I,
    typename std::remove_reference<Tuple>::type>::type>::type;

template <std::size_t I, typename Tuple>
constexpr auto tuple_get(Tuple &tuple) -> decltype(std::get<I>(tuple)) {
  return std::get<I>(tuple);
}

// ad hoc: older libstdc++ releases lack the corrected const-rvalue `std::get`
// overload. Restore the tuple element's forwarded category explicitly.
template <std::size_t I, typename Tuple>
constexpr auto tuple_get(Tuple &&tuple) //
  -> typename std::enable_if<
    !std::is_lvalue_reference<Tuple>::value,
    tuple_element_rvalue_t<I, Tuple>>::type {
  return static_cast<tuple_element_rvalue_t<I, Tuple>>(std::get<I>(tuple));
}

template <typename Visit, typename Tuple, std::size_t... I>
constexpr auto _apply(Visit &&v, Tuple &&t, index_sequence<I...>)
  -> decltype(std::forward<Visit>(v)(
    tuple_get<I>(std::forward<Tuple>(t))...)) {
  return std::forward<Visit>(v)(tuple_get<I>(std::forward<Tuple>(t))...);
}

template <typename Visit, typename Tuple>
constexpr auto apply(Visit &&v, Tuple &&t)
  -> decltype(_apply(std::forward<Visit>(v),
    std::forward<Tuple>(t),
    make_integer_sequence<std::size_t,
      std::tuple_size<decay_t<Tuple>>::value>{})) {
  return _apply(std::forward<Visit>(v),
    std::forward<Tuple>(t),
    make_integer_sequence<std::size_t,
      std::tuple_size<decay_t<Tuple>>::value>{});
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

namespace detail {

// Unqualified access supports arrays, member functions, and ADL-only ranges.
using std::begin;
using std::end;

template <typename Range>
using range_iterator_t = decltype(begin(std::declval<Range &>()));

template <typename Range>
using range_comparison_t = decltype(bool(begin(std::declval<Range &>())
  != end(std::declval<Range &>())));

template <typename Range>
using range_reference_t = decltype(*std::declval<range_iterator_t<Range> &>());

template <typename Range>
using range_value_t = typename std::iterator_traits<
  range_iterator_t<Range>>::value_type;

/** Detect input ranges using legacy iterator categories and operations.
 * Semantic iterator guarantees remain the range author's responsibility. */
template <typename Range, typename = void>
struct is_input_range: std::false_type {};

template <typename Range>
struct is_input_range<Range,
  void_t<typename std::iterator_traits<
      range_iterator_t<Range>>::iterator_category,
    range_reference_t<Range>,
    decltype(++std::declval<range_iterator_t<Range> &>()),
    decltype(std::declval<range_iterator_t<Range> &>()++),
    range_comparison_t<Range>>>:
    std::is_base_of<std::input_iterator_tag,
      typename std::iterator_traits<
        range_iterator_t<Range>>::iterator_category> {};

} // namespace detail

// Use the standard range concept when the library provides C++20 ranges.
#if defined(__cpp_lib_ranges) && 201911L <= __cpp_lib_ranges
template <typename Range>
using range_reference_t = std::ranges::range_reference_t<Range>;

template <typename Range>
using range_value_t = std::ranges::range_value_t<Range>;

template <typename Range>
struct is_input_range: std::bool_constant<std::ranges::input_range<Range>> {};
#else
using detail::range_reference_t;
using detail::range_value_t;
using detail::is_input_range;
#endif

// apply
// ad hoc: libstdc++ before release 9 does not preserve const-rvalue tuple
// access through std::apply. Use the corrected local tuple access there.
#if OMNI_CPLUSPLUS >= 201703L \
  && (!defined(_GLIBCXX_RELEASE) || 9 <= _GLIBCXX_RELEASE)
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

// conjunction
#if defined(__cpp_lib_logical_traits) && 201510L <= __cpp_lib_logical_traits
using std::conjunction;
#else
template <typename...>
struct conjunction: std::true_type {};

template <typename First>
struct conjunction<First>: First {};

template <typename First, typename... Rest>
struct conjunction<First, Rest...>:
    conditional_t<bool(First::value), conjunction<Rest...>, First> {};
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
    conditional_t<bool(B1::value), B1, disjunction<Bn...>> {};
#endif

// negation
#if OMNI_CPLUSPLUS >= 201703L
using std::negation;
#else
template <typename Trait>
struct negation: std::integral_constant<bool, !bool(Trait::value)> {};
#endif

// invoke
#if defined(__cpp_lib_constexpr_functional) \
  && 201907L <= __cpp_lib_constexpr_functional
using std::invoke;
#else
namespace detail {

template <typename T>
struct is_reference_wrapper: std::false_type {};

template <typename T>
struct is_reference_wrapper<std::reference_wrapper<T>>: std::true_type {};

template <typename Class, typename Object>
struct is_member_owner:
    disjunction<std::is_same<Class, decay_t<Object>>,
      std::is_base_of<Class, decay_t<Object>>> {};

} // namespace detail

template <typename Function, typename... Argument>
constexpr auto invoke(Function &&function, Argument &&...argument) noexcept(
  noexcept(
    std::forward<Function>(function)(std::forward<Argument>(argument)...))) //
  -> typename std::enable_if<!std::is_member_pointer<decay_t<Function>>::value,
    decltype(std::forward<Function>(function)(
      std::forward<Argument>(argument)...))>::type {
  return std::forward<Function>(function)(std::forward<Argument>(argument)...);
}

template <typename Member,
  typename Class,
  typename Object,
  typename... Argument>
constexpr auto invoke(Member Class::*member,
  Object &&object,
  Argument &&...argument) noexcept( //
  noexcept((std::forward<Object>(object).*member)(
    std::forward<Argument>(argument)...))) //
  -> typename std::enable_if<std::is_function<Member>::value
      && detail::is_member_owner<Class, Object>::value,
    decltype((std::forward<Object>(object).*member)(
      std::forward<Argument>(argument)...))>::type {
  return (std::forward<Object>(object).*member)(
    std::forward<Argument>(argument)...);
}

template <typename Member,
  typename Class,
  typename Object,
  typename... Argument>
constexpr auto invoke(Member Class::*member,
  Object &&object,
  Argument &&...argument) noexcept( //
  noexcept((std::forward<Object>(object).get().*member)(
    std::forward<Argument>(argument)...))) //
  -> typename std::enable_if<std::is_function<Member>::value
      && !detail::is_member_owner<Class, Object>::value
      && detail::is_reference_wrapper<decay_t<Object>>::value,
    decltype((std::forward<Object>(object).get().*member)(
      std::forward<Argument>(argument)...))>::type {
  return (std::forward<Object>(object).get().*member)(
    std::forward<Argument>(argument)...);
}

template <typename Member,
  typename Class,
  typename Object,
  typename... Argument>
constexpr auto invoke(Member Class::*member,
  Object &&object,
  Argument &&...argument) noexcept( //
  noexcept(((*std::forward<Object>(object)).*member)(
    std::forward<Argument>(argument)...))) //
  -> typename std::enable_if<std::is_function<Member>::value
      && !detail::is_member_owner<Class, Object>::value
      && !detail::is_reference_wrapper<decay_t<Object>>::value,
    decltype(((*std::forward<Object>(object)).*member)(
      std::forward<Argument>(argument)...))>::type {
  return ((*std::forward<Object>(object)).*member)(
    std::forward<Argument>(argument)...);
}

template <typename Member, typename Class, typename Object>
constexpr auto invoke(Member Class::*member, Object &&object) noexcept(
  noexcept(std::forward<Object>(object).*member)) ->
  typename std::enable_if<std::is_object<Member>::value
      && detail::is_member_owner<Class, Object>::value,
    decltype(std::forward<Object>(object).*member)>::type {
  return std::forward<Object>(object).*member;
}

template <typename Member, typename Class, typename Object>
constexpr auto invoke(Member Class::*member, Object &&object) noexcept(
  noexcept(std::forward<Object>(object).get().*member)) ->
  typename std::enable_if<std::is_object<Member>::value
      && !detail::is_member_owner<Class, Object>::value
      && detail::is_reference_wrapper<decay_t<Object>>::value,
    decltype(std::forward<Object>(object).get().*member)>::type {
  return std::forward<Object>(object).get().*member;
}

template <typename Member, typename Class, typename Object>
constexpr auto invoke(Member Class::*member, Object &&object) noexcept(
  noexcept((*std::forward<Object>(object)).*member)) //
  -> typename std::enable_if<std::is_object<Member>::value
      && !detail::is_member_owner<Class, Object>::value
      && !detail::is_reference_wrapper<decay_t<Object>>::value,
    decltype((*std::forward<Object>(object)).*member)>::type {
  return (*std::forward<Object>(object)).*member;
}
#endif

// invoke_result_t
#if defined(__cpp_lib_is_invocable) && 201703L <= __cpp_lib_is_invocable
using std::invoke_result_t;
#else
template <typename Function, typename... Argument>
using invoke_result_t = decltype(compat::invoke(std::declval<Function>(),
  std::declval<Argument>()...));
#endif

// is_invocable
#if defined(__cpp_lib_is_invocable) && 201703L <= __cpp_lib_is_invocable
using std::is_invocable;
#else
namespace detail {

template <typename, typename Function, typename... Argument>
struct is_invocable: std::false_type {};

template <typename Function, typename... Argument>
struct is_invocable< //
  void_t<decltype(compat::invoke(std::declval<Function>(),
    std::declval<Argument>()...))>,
  Function,
  Argument...>: std::true_type {};

} // namespace detail

template <typename Function, typename... Argument>
struct is_invocable: detail::is_invocable<void, Function, Argument...> {};
#endif

#if defined(__cpp_concepts) && 201907L <= __cpp_concepts
template <typename Function, typename... Argument>
concept invocable = is_invocable<Function, Argument...>::value;
#endif

// enable_if_t
#if defined(__cpp_lib_transformation_trait_aliases) \
  && 201304L <= __cpp_lib_transformation_trait_aliases
using std::enable_if_t;
#else
template <bool Condition, typename T = void>
using enable_if_t = typename std::enable_if<Condition, T>::type;
#endif

// bind_front
// libstdc++ 9 advertises bind_front before std::invoke supports constant
// evaluation. GCC 16.1 also aborts compilation with an internal compiler error
// while forming a binder for compile-time branch closures. Use the
// compatibility implementation in both cases.
#if defined(__cpp_lib_bind_front) && 201907L <= __cpp_lib_bind_front \
  && defined(__cpp_lib_constexpr_functional) \
  && 201907L <= __cpp_lib_constexpr_functional \
  && !(defined(__GNUC__) && !defined(__clang__) && 16 == __GNUC__ \
    && 1 == __GNUC_MINOR__)
using std::bind_front;
#else
namespace detail {

// libc++ does not provide constexpr std::tuple construction in C++11. This
// private indexed storage keeps the compatibility implementation constexpr.
template <std::size_t Index, typename... Value>
struct bound_type_at;

template <typename First, typename... Rest>
struct bound_type_at<0, First, Rest...> {
  using type = First;
};

template <std::size_t Index, typename First, typename... Rest>
struct bound_type_at<Index, First, Rest...>:
    bound_type_at<Index - 1, Rest...> {};

template <std::size_t Index, typename Value>
struct bound_value {
  Value value;

  template <typename Source>
  constexpr bound_value(Source &&source): value{std::forward<Source>(source)} {}
};

template <typename Index, typename... Value>
struct bound_values;

template <std::size_t... Index, typename... Value>
struct bound_values<index_sequence<Index...>, Value...>:
    bound_value<Index, Value>... {
  template <typename... Source>
  constexpr bound_values(Source &&...source)
      : bound_value<Index, Value>{std::forward<Source>(source)}... {}
};

template <std::size_t Index, typename Indices, typename... Value>
constexpr typename bound_type_at<Index, Value...>::type &bound_get(
  bound_values<Indices, Value...> &values) noexcept {
  using element =
    bound_value<Index, typename bound_type_at<Index, Value...>::type>;
  return static_cast<element &>(values).value;
}

template <std::size_t Index, typename Indices, typename... Value>
constexpr const typename bound_type_at<Index, Value...>::type &bound_get(
  const bound_values<Indices, Value...> &values) noexcept {
  using element =
    bound_value<Index, typename bound_type_at<Index, Value...>::type>;
  return static_cast<const element &>(values).value;
}

template <std::size_t Index, typename Indices, typename... Value>
constexpr typename bound_type_at<Index, Value...>::type &&bound_get(
  bound_values<Indices, Value...> &&values) noexcept {
  using element =
    bound_value<Index, typename bound_type_at<Index, Value...>::type>;
  return std::move(static_cast<element &>(values).value);
}

template <std::size_t Index, typename Indices, typename... Value>
constexpr const typename bound_type_at<Index, Value...>::type &&bound_get(
  const bound_values<Indices, Value...> &&values) noexcept {
  using element =
    bound_value<Index, typename bound_type_at<Index, Value...>::type>;
  return std::move(static_cast<const element &>(values).value);
}

template <typename Function, typename... Bound>
struct bind_front_t {
  using indices = make_index_sequence<sizeof...(Bound)>;

  Function function;
  bound_values<indices, Bound...> bound;

  template <typename Source,
    typename std::enable_if<
      !std::is_same<bind_front_t, decay_t<Source>>::value,
      int>::type = 0,
    typename... Value>
  constexpr bind_front_t(Source &&source, Value &&...value)
      : function{std::forward<Source>(source)}
      , bound{std::forward<Value>(value)...} {}

  template <typename Self, std::size_t... Index, typename... Argument>
  static constexpr auto call(Self &&self,
    index_sequence<Index...>,
    Argument &&...argument) //
    noexcept(noexcept(compat::invoke(std::forward<Self>(self).function,
      bound_get<Index>(std::forward<Self>(self).bound)...,
      std::forward<Argument>(argument)...)))
      -> decltype(compat::invoke(std::forward<Self>(self).function,
        bound_get<Index>(std::forward<Self>(self).bound)...,
        std::forward<Argument>(argument)...)) {
    return compat::invoke(std::forward<Self>(self).function,
      bound_get<Index>(std::forward<Self>(self).bound)...,
      std::forward<Argument>(argument)...);
  }

  template <typename... Argument>
  OMNI_CPP14_CONSTEXPR auto operator()(Argument &&...argument) & //
    noexcept(noexcept(call(std::declval<bind_front_t &>(),
      indices{},
      std::forward<Argument>(argument)...)))
      -> decltype(call(std::declval<bind_front_t &>(),
        indices{},
        std::forward<Argument>(argument)...)) {
    return call(*this, indices{}, std::forward<Argument>(argument)...);
  }

  template <typename... Argument>
  constexpr auto operator()(Argument &&...argument) const & //
    noexcept(noexcept(call(std::declval<const bind_front_t &>(),
      indices{},
      std::forward<Argument>(argument)...)))
      -> decltype(call(std::declval<const bind_front_t &>(),
        indices{},
        std::forward<Argument>(argument)...)) {
    return call(*this, indices{}, std::forward<Argument>(argument)...);
  }

  template <typename... Argument>
  OMNI_CPP14_CONSTEXPR auto operator()(Argument &&...argument) && //
    noexcept(noexcept(call(std::declval<bind_front_t &&>(),
      indices{},
      std::forward<Argument>(argument)...)))
      -> decltype(call(std::declval<bind_front_t &&>(),
        indices{},
        std::forward<Argument>(argument)...)) {
    return call(std::move(*this),
      indices{},
      std::forward<Argument>(argument)...);
  }

  template <typename... Argument>
  constexpr auto operator()(Argument &&...argument) const && //
    noexcept(noexcept(call(std::declval<const bind_front_t &&>(),
      indices{},
      std::forward<Argument>(argument)...)))
      -> decltype(call(std::declval<const bind_front_t &&>(),
        indices{},
        std::forward<Argument>(argument)...)) {
    return call(std::move(*this),
      indices{},
      std::forward<Argument>(argument)...);
  }
};

} // namespace detail

template <typename Function, typename... Bound>
constexpr auto bind_front(Function &&function, Bound &&...bound) //
  -> typename std::enable_if<
    std::is_constructible<decay_t<Function>, Function &&>::value
      && std::is_move_constructible<decay_t<Function>>::value
      && std::is_constructible<std::tuple<decay_t<Bound>...>,
        Bound &&...>::value
      && std::is_move_constructible<std::tuple<decay_t<Bound>...>>::value,
    detail::bind_front_t<decay_t<Function>, decay_t<Bound>...>>::type {
  return detail::bind_front_t<decay_t<Function>, decay_t<Bound>...>{
    std::forward<Function>(function),
    std::forward<Bound>(bound)...};
}
#endif

// make_unique
// FIXME: [low] Freestanding: keep allocating helpers separate; make_unique is
// not part of the guaranteed freestanding <memory> subset.
#if OMNI_CPLUSPLUS >= 201402L
using std::make_unique;
#else
template <typename T, typename... Argument>
typename std::enable_if<!std::is_array<T>::value, std::unique_ptr<T>>::type
  make_unique(Argument &&...argument) {
  return std::unique_ptr<T>{new T(std::forward<Argument>(argument)...)};
}

template <typename T>
typename std::enable_if<std::is_array<T>::value && 0 == std::extent<T>::value,
  std::unique_ptr<T>>::type
  make_unique(std::size_t size) {
  using element = typename std::remove_extent<T>::type;
  return std::unique_ptr<T>{new element[size]()};
}

template <typename T, typename... Argument>
typename std::enable_if<0 != std::extent<T>::value>::type make_unique(
  Argument &&...) = delete;
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
