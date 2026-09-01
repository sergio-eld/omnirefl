#pragma once

#include <omnirefl/compat.hpp>
#include <omnirefl/traits.hpp>

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

// Clang 22 accepts captureless lambdas as non-type template arguments but
// still reports __cpp_nontype_template_args == 201411L.
#if defined(__cpp_generic_lambdas) && 201707L <= __cpp_generic_lambdas \
  && 202002L <= OMNI_CPLUSPLUS
#  define OMNI_FN_HAS_PREDICATE_LAMBDA 1
#else
#  define OMNI_FN_HAS_PREDICATE_LAMBDA 0
#endif

namespace omni {

#if defined(OMNI_TYPE_T_DEFINED)
template <typename T>
struct type_t;
#else
// `type_t` is an instrumentation primitive recognized by the reflection tool.
// todo: consider moving its shared definition to a standalone header.
template <typename T>
struct type_t {
  using type = T;
};
#  define OMNI_TYPE_T_DEFINED
#endif

namespace fn {
namespace detail {

// Keep language-version selection away from the public declarations so LSP
// hover preserves their documentation.
#if !defined(__cpp_generic_lambdas) || __cpp_generic_lambdas < 201304L
// C++11 emulation of the generic lambda returned by `ctad`.
template <template <typename...> class Template>
struct cpp11_type_template_ctad {
  template <typename Value,
    typename std::enable_if<
      traits::is_type_template_constructible_from<Template, Value &&>::value,
      int>::type = 0>
  constexpr traits::type_template_construct_result_t<Template, Value &&>
    operator()(Value &&value) const {
    return Template<compat::decay_t<Value>>{std::forward<Value>(value)};
  }
};

template <template <typename...> class Template>
constexpr cpp11_type_template_ctad<Template> type_template_ctad() {
  return {};
}
#elif defined(__cpp_deduction_guides) && 201703L <= __cpp_deduction_guides
template <template <typename...> class Template>
constexpr auto type_template_ctad() {
#  if defined(__cpp_concepts) && 201907L <= __cpp_concepts
  return //
    []<typename Value>(Value &&value)
    requires traits::type_template_constructible_from<Template, Value &&>
  { return Template{std::forward<Value>(value)}; };
#  else
  return //
    [](auto &&value) //
    -> typename std::enable_if<
      traits::is_type_template_constructible_from<Template,
        decltype(value)>::value,
      traits::type_template_construct_result_t<Template,
        decltype(value)>>::type {
      return Template{std::forward<decltype(value)>(value)};
    };
#  endif
}

template <template <typename, std::size_t> class Template>
constexpr auto type_size_template_ctad() {
#  if defined(__cpp_concepts) && 201907L <= __cpp_concepts
  return //
    []<typename Value>(Value &&value)
    requires traits::type_size_template_constructible_from<Template, Value &&>
  { return Template{std::forward<Value>(value)}; };
#  else
  return //
    [](auto &&value) //
    -> typename std::enable_if<
      traits::is_type_size_template_constructible_from<Template,
        decltype(value)>::value,
      traits::type_size_template_construct_result_t<Template,
        decltype(value)>>::type {
      return Template{std::forward<decltype(value)>(value)};
    };
#  endif
}
#else
template <template <typename...> class Template>
constexpr auto type_template_ctad() {
  return //
    [](auto &&value) //
    -> typename std::enable_if<
      traits::is_type_template_constructible_from<Template,
        decltype(value)>::value,
      traits::type_template_construct_result_t<Template,
        decltype(value)>>::type {
      return Template<compat::decay_t<decltype(value)>>{
        std::forward<decltype(value)>(value)};
    };
}

#endif

} // namespace detail

/// Return a conversion using CTAD or its value-type fallback.
///
/// This overload accepts class templates containing only type template
/// parameters.
template <template <typename...> class Template>
constexpr decltype(detail::type_template_ctad<Template>()) ctad() {
  return detail::type_template_ctad<Template>();
}

#if defined(__cpp_deduction_guides) && 201703L <= __cpp_deduction_guides
/// Return a CTAD conversion for a class template whose parameters are one type
/// followed by one `std::size_t`.
template <template <typename, std::size_t> class Template>
constexpr decltype(detail::type_size_template_ctad<Template>()) ctad() {
  return detail::type_size_template_ctad<Template>();
}
#endif

#if defined(__cpp_concepts) && 201907L <= __cpp_concepts
/// Construct the type selected by an Omnirefl type tag.
template <template <typename> class Tag, typename To, typename Value>
  requires(
    traits::is<type_t, Tag>() && traits::brace_constructible_from<To, Value &&>)
#else
/// Construct the type selected by an Omnirefl type tag.
template <template <typename> class Tag,
  typename To,
  typename Value,
  typename std::enable_if<traits::is<type_t, Tag>()
      && traits::is_brace_constructible<To, Value &&>::value,
    int>::type = 0>
#endif
constexpr To as(Tag<To>, Value &&value) {
  return To{std::forward<Value>(value)};
}

#if defined(__cpp_concepts) && 201907L <= __cpp_concepts
/// Convert a forwarded value with a local copy or move of `conversion`.
template <typename Conversion, typename Value>
  requires compat::invocable<Conversion &, Value &&>
#else
/// Convert a forwarded value with a local copy or move of `conversion`.
template <typename Conversion,
  typename Value,
  typename std::enable_if<compat::is_invocable<Conversion &, Value &&>::value,
    int>::type = 0>
#endif
constexpr auto as(Conversion conversion, Value &&value)
  -> decltype(compat::invoke(conversion, std::forward<Value>(value))) {
  return compat::invoke(conversion, std::forward<Value>(value));
}

template <typename Predicate>
struct not_closure;

namespace detail {

template <typename Tuple, typename = void>
struct is_tuple_like: std::false_type {};

template <typename Tuple>
struct is_tuple_like<Tuple,
  compat::void_t<decltype(std::tuple_size<Tuple>::value)>>: std::true_type {};

template <typename Predicate>
struct is_type_callable: std::is_empty<Predicate> {};

template <typename Predicate>
struct is_type_callable<not_closure<Predicate>>: is_type_callable<Predicate> {};

template <typename Predicate,
  typename Tuple,
  typename Remaining,
  typename Selected = compat::index_sequence<>>
struct filtered_indices;

template <typename Predicate,
  typename Tuple,
  std::size_t Index,
  std::size_t... Remaining,
  std::size_t... Selected>
struct filtered_indices<Predicate,
  Tuple,
  compat::index_sequence<Index, Remaining...>,
  compat::index_sequence<Selected...>>:
    filtered_indices<Predicate,
      Tuple,
      compat::index_sequence<Remaining...>,
      compat::conditional_t<Predicate{}.template
        operator()<decltype(std::get<Index>(std::declval<Tuple>()))>(),
        compat::index_sequence<Selected..., Index>,
        compat::index_sequence<Selected...>>> {};

template <typename Predicate, typename Tuple, std::size_t... Selected>
struct filtered_indices<Predicate,
  Tuple,
  compat::index_sequence<>,
  compat::index_sequence<Selected...>> {
  static_assert(is_type_callable<Predicate>::value,
    "filter predicates must encode selection in their type");
  static_assert(std::is_default_constructible<Predicate>::value,
    "filter predicates must be default constructible");

  using type = compat::index_sequence<Selected...>;
};

template <typename Tuple, std::size_t... Index>
constexpr auto select_tuple(Tuple &&tuple, compat::index_sequence<Index...>)
  -> std::tuple<
    compat::decay_t<decltype(std::get<Index>(std::forward<Tuple>(tuple)))>...> {
  return {std::get<Index>(std::forward<Tuple>(tuple))...};
}

template <typename Predicate, typename Tuple>
constexpr auto filter_values(Tuple &&tuple)
  -> decltype(select_tuple(std::forward<Tuple>(tuple),
    typename filtered_indices<Predicate,
      Tuple &&,
      compat::make_index_sequence<
        std::tuple_size<compat::remove_cvref_t<Tuple>>::value>>::type{})) {
  return select_tuple(std::forward<Tuple>(tuple),
    typename filtered_indices<Predicate,
      Tuple &&,
      compat::make_index_sequence<
        std::tuple_size<compat::remove_cvref_t<Tuple>>::value>>::type{});
}

template <std::size_t Index, std::size_t Size>
struct any_of_values {
  template <typename Predicate, typename Tuple>
  static constexpr bool call(Predicate &predicate, Tuple &&tuple) {
    return compat::invoke(predicate,
             std::get<Index>(std::forward<Tuple>(tuple)))
      || any_of_values<Index + 1, Size>::call(predicate,
        std::forward<Tuple>(tuple));
  }
};

template <std::size_t Size>
struct any_of_values<Size, Size> {
  template <typename Predicate, typename Tuple>
  static constexpr bool call(Predicate &, Tuple &&) {
    return false;
  }
};

template <typename Key, typename Left, typename Right, typename Remaining>
struct matches_any;

template <typename Key,
  typename Left,
  typename Right,
  std::size_t Index,
  std::size_t... Remaining>
struct matches_any<Key,
  Left,
  Right,
  compat::index_sequence<Index, Remaining...>>:
    std::integral_constant<bool,
      Key{}.template operator()<Left>()
          == Key{}.template
          operator()<decltype(std::get<Index>(std::declval<Right>()))>()
        || matches_any<Key,
          Left,
          Right,
          compat::index_sequence<Remaining...>>::value> {};

template <typename Key, typename Left, typename Right>
struct matches_any<Key, Left, Right, compat::index_sequence<>>:
    std::false_type {};

template <typename Key,
  typename Left,
  typename Right,
  typename Remaining,
  typename Selected = compat::index_sequence<>>
struct diff_indices;

template <typename Key,
  typename Left,
  typename Right,
  std::size_t Index,
  std::size_t... Remaining,
  std::size_t... Selected>
struct diff_indices<Key,
  Left,
  Right,
  compat::index_sequence<Index, Remaining...>,
  compat::index_sequence<Selected...>>:
    diff_indices<Key,
      Left,
      Right,
      compat::index_sequence<Remaining...>,
      compat::conditional_t< //
        matches_any<Key,
          decltype(std::get<Index>(std::declval<Left>())),
          Right,
          compat::make_index_sequence<
            std::tuple_size<compat::remove_cvref_t<Right>>::value>>::value,
        compat::index_sequence<Selected...>,
        compat::index_sequence<Selected..., Index>>> {};

template <typename Key, typename Left, typename Right, std::size_t... Selected>
struct diff_indices<Key,
  Left,
  Right,
  compat::index_sequence<>,
  compat::index_sequence<Selected...>> {
  static_assert(is_type_callable<Key>::value,
    "diff_by keys must encode projection in their type");
  static_assert(std::is_default_constructible<Key>::value,
    "diff_by keys must be default constructible");

  using type = compat::index_sequence<Selected...>;
};

template <typename Key, typename Left, typename Right>
constexpr auto diff_by_values(Left &&left, Right &&)
  -> decltype(select_tuple(std::forward<Left>(left),
    typename diff_indices<Key,
      Left &&,
      Right &&,
      compat::make_index_sequence<
        std::tuple_size<compat::remove_cvref_t<Left>>::value>>::type{})) {
  return select_tuple(std::forward<Left>(left),
    typename diff_indices<Key,
      Left &&,
      Right &&,
      compat::make_index_sequence<
        std::tuple_size<compat::remove_cvref_t<Left>>::value>>::type{});
}

constexpr bool same_field_name(const char *left, const char *right) noexcept {
  return *left == *right
    && ('\0' == *left || same_field_name(left + 1, right + 1));
}

template <typename Field>
struct field_name_key {};

template <typename Left, typename Right>
constexpr bool operator==(field_name_key<Left>, field_name_key<Right>) {
  return same_field_name(compat::remove_cvref_t<Left>::name(),
    compat::remove_cvref_t<Right>::name());
}

// An empty result lets `each` remain constexpr under C++11 while preserving
// its discard-only semantics.
struct discard_result {
  template <typename... Value>
  constexpr discard_result(Value &&...) {}
};

#if !defined(__cpp_constexpr) || __cpp_constexpr < 201603L
// C++11 emulation of the constexpr generic lambda used by `each`. This is
// retained through C++14 because lambdas did not become constexpr until C++17.
template <typename Visit>
struct cpp11_visit_each {
  Visit &visit;

  template <typename... Element>
  constexpr discard_result operator()(Element &&...element) const {
    return discard_result{
      (static_cast<void>(compat::invoke(visit, std::forward<Element>(element))),
        0)...,
    };
  }
};

// C++11 emulation of the constexpr generic lambda used by `map`.
template <typename Visit>
struct cpp11_map_visit {
  Visit &visit;

  template <typename... Element>
  constexpr auto operator()(Element &&...element) const
    -> std::tuple<compat::decay_t<decltype(compat::invoke(visit,
      std::forward<Element>(element)))>...> {
    return std::tuple<compat::decay_t<decltype(compat::invoke(visit,
      std::forward<Element>(element)))>...>{
      compat::invoke(visit, std::forward<Element>(element))...};
  }
};

template <typename Accumulator, typename Visit>
constexpr Accumulator foldl_values(Visit &, Accumulator accumulator) {
  return accumulator;
}

// C++11 constant expressions reject assignment, so the left fold uses
// expression-only recursion.
template <typename Accumulator,
  typename Visit,
  typename Element,
  typename... Rest>
constexpr Accumulator foldl_values(Visit &visit,
  Accumulator accumulator,
  Element &&element,
  Rest &&...rest) {
  return foldl_values<Accumulator>(visit,
    compat::invoke(visit,
      std::move(accumulator),
      std::forward<Element>(element)),
    std::forward<Rest>(rest)...);
}

// C++11 emulation of the constexpr generic lambda used by `foldl`.
template <typename Visit, typename Accumulator>
struct cpp11_foldl_visit {
  Visit &visit;
  Accumulator &accumulator;

  template <typename... Element>
  constexpr Accumulator operator()(Element &&...element) const {
    return foldl_values<Accumulator>(visit,
      std::move(accumulator),
      std::forward<Element>(element)...);
  }
};
#endif

// A right fold needs a named recursive visitor because a parameter pack cannot
// be expanded in reverse.
template <typename Visit, typename Accumulator>
struct foldr_visit {
  Visit &visit;
  Accumulator &accumulator;

  constexpr Accumulator operator()() const {
    return std::move(accumulator);
  }

  template <typename First, typename... Element>
  constexpr Accumulator operator()(First &&first, Element &&...element) const {
    return compat::invoke(visit,
      std::forward<First>(first),
      (*this)(std::forward<Element>(element)...));
  }
};

#if defined(__cpp_concepts) && 201907L <= __cpp_concepts
template <typename Tuple>
concept tuple_like = is_tuple_like<compat::remove_cvref_t<Tuple>>::value;
#endif

} // namespace detail

// TODO: Consider returning the final unary visitor like `std::for_each`.
#if defined(__cpp_concepts) && 201907L <= __cpp_concepts
template <typename Visit, detail::tuple_like Tuple>
#else
template <typename Visit,
  typename Tuple,
  typename std::enable_if<
    detail::is_tuple_like<compat::remove_cvref_t<Tuple>>::value,
    int>::type = 0>
#endif
/// Invoke a local copy or move of `visit` once for each element of a standard
/// tuple-like object.
///
/// The tuple-like protocol is used. General ranges and containers are outside
/// this utility's contract; `each` avoids implying the broader support commonly
/// associated with `for_each`.
constexpr detail::discard_result each(Visit visit, Tuple &&tuple) {
#if defined(__cpp_constexpr) && 201603L <= __cpp_constexpr \
  && defined(__cpp_generic_lambdas)
  return compat::apply(
    [&visit](auto &&...element) constexpr {
      return detail::discard_result{
        (static_cast<void>(
           compat::invoke(visit, std::forward<decltype(element)>(element))),
          0)...,
      };
    },
    std::forward<Tuple>(tuple));
#else
  return compat::apply(detail::cpp11_visit_each<Visit>{visit},
    std::forward<Tuple>(tuple));
#endif
}

#if defined(__cpp_concepts) && 201907L <= __cpp_concepts
template <detail::tuple_like Left, detail::tuple_like Right>
#else
template <typename Left,
  typename Right,
  typename std::enable_if<
    detail::is_tuple_like<compat::remove_cvref_t<Left>>::value
      && detail::is_tuple_like<compat::remove_cvref_t<Right>>::value,
    int>::type = 0>
#endif
/// Concatenate two standard tuple-like objects into an owning tuple.
constexpr auto concat(Left &&left, Right &&right)
  -> decltype(std::tuple_cat(std::forward<Left>(left),
    std::forward<Right>(right))) {
  return std::tuple_cat(std::forward<Left>(left), std::forward<Right>(right));
}

#if defined(__cpp_concepts) && 201907L <= __cpp_concepts
template <typename Predicate, detail::tuple_like Tuple>
#else
template <typename Predicate,
  typename Tuple,
  typename std::enable_if<
    detail::is_tuple_like<compat::remove_cvref_t<Tuple>>::value,
    int>::type = 0>
#endif
/// Return whether `predicate` accepts any tuple element.
constexpr bool any_of(Predicate predicate, Tuple &&tuple) {
  return detail::any_of_values<0,
    std::tuple_size<compat::remove_cvref_t<Tuple>>::value>::call(predicate,
    std::forward<Tuple>(tuple));
}

#if defined(__cpp_concepts) && 201907L <= __cpp_concepts
template <typename Predicate, detail::tuple_like Tuple>
#else
template <typename Predicate,
  typename Tuple,
  typename std::enable_if<
    detail::is_tuple_like<compat::remove_cvref_t<Tuple>>::value,
    int>::type = 0>
#endif
/// Return whether `predicate` rejects every tuple element.
constexpr bool none_of(Predicate predicate, Tuple &&tuple) {
  return !any_of(std::move(predicate), std::forward<Tuple>(tuple));
}

/// Invoke `predicate` and negate its result.
template <typename Predicate, typename First, typename... Argument>
constexpr auto not_(Predicate predicate, First &&first, Argument &&...argument)
  -> decltype(!compat::invoke(predicate,
    std::forward<First>(first),
    std::forward<Argument>(argument)...)) {
  return !compat::invoke(predicate,
    std::forward<First>(first),
    std::forward<Argument>(argument)...);
}

/// QoL key projection for comparing reflected fields by name.
// TODO: Constrain Field to reflected field metadata or field bindings.
struct field_name {
  template <typename Field>
  constexpr detail::field_name_key<Field> operator()() const {
    return {};
  }
};

#if defined(__cpp_concepts) && 201907L <= __cpp_concepts
template <typename Key, detail::tuple_like Left, detail::tuple_like Right>
#else
template <typename Key,
  typename Left,
  typename Right,
  typename std::enable_if<
    detail::is_tuple_like<compat::remove_cvref_t<Left>>::value
      && detail::is_tuple_like<compat::remove_cvref_t<Right>>::value,
    int>::type = 0>
#endif
/// Select left tuple elements unmatched by any right tuple element.
///
/// `Key` follows the type-projection protocol with `operator()<Element>()`.
/// `Element` preserves the access category produced by its forwarded tuple;
/// projected keys must support constexpr equality.
constexpr auto diff_by(Key, Left &&left, Right &&right)
  -> decltype(detail::diff_by_values<Key>(std::forward<Left>(left),
    std::forward<Right>(right))) {
  return detail::diff_by_values<Key>(std::forward<Left>(left),
    std::forward<Right>(right));
}

#if defined(__cpp_concepts) && 201907L <= __cpp_concepts
template <typename Predicate, detail::tuple_like Tuple>
#else
template <typename Predicate,
  typename Tuple,
  typename std::enable_if<
    detail::is_tuple_like<compat::remove_cvref_t<Tuple>>::value,
    int>::type = 0>
#endif
/// Select tuple elements whose access types satisfy `Predicate`.
///
/// `Predicate` must be an empty, default-constructible type with a constexpr
/// `operator()<Element>()`. `Element` preserves the cv/ref category produced by
/// accessing the forwarded tuple. Selected values retain their order and are
/// forwarded into an owning tuple.
constexpr auto filter(Predicate, Tuple &&tuple)
  -> decltype(detail::filter_values<Predicate>(std::forward<Tuple>(tuple))) {
  return detail::filter_values<Predicate>(std::forward<Tuple>(tuple));
}

#if OMNI_FN_HAS_PREDICATE_LAMBDA
/// Adapt a C++20 templated lambda to the type-predicate protocol.
template <auto Predicate>
struct predicate_t {
  template <typename Element>
  constexpr bool operator()() const {
    return Predicate.template operator()<Element>();
  }
};

template <auto Predicate>
inline constexpr predicate_t<Predicate> pred{};
#endif

#if defined(__cpp_concepts) && 201907L <= __cpp_concepts
template <typename Visit, detail::tuple_like Tuple>
#else
template <typename Visit,
  typename Tuple,
  typename std::enable_if<
    detail::is_tuple_like<compat::remove_cvref_t<Tuple>>::value,
    int>::type = 0>
#endif
#if defined(__cpp_constexpr) && 201603L <= __cpp_constexpr \
  && defined(__cpp_generic_lambdas)
/// Invoke a local copy or move of `visit` for each tuple element and return
/// the results as a tuple of values.
constexpr auto map(Visit visit, Tuple &&tuple) {
  return compat::apply(
    [&visit](auto &&...element) constexpr {
      return std::tuple<compat::decay_t<decltype(compat::invoke(visit,
        std::forward<decltype(element)>(element)))>...>{
        compat::invoke(visit, std::forward<decltype(element)>(element))...};
    },
    std::forward<Tuple>(tuple));
}
#else
/// Invoke a local copy or move of `visit` for each tuple element and return
/// the results as a tuple of values.
constexpr auto map(Visit visit, Tuple &&tuple)
  -> decltype(compat::apply(detail::cpp11_map_visit<Visit>{visit},
    std::forward<Tuple>(tuple))) {
  return compat::apply(detail::cpp11_map_visit<Visit>{visit},
    std::forward<Tuple>(tuple));
}
#endif

#if defined(__cpp_concepts) && 201907L <= __cpp_concepts
template <typename Visit, typename Accumulator, detail::tuple_like Tuple>
#else
template <typename Visit,
  typename Accumulator,
  typename Tuple,
  typename std::enable_if<
    detail::is_tuple_like<compat::remove_cvref_t<Tuple>>::value,
    int>::type = 0>
#endif
/// Left-fold a tuple with local copies or moves of a binary visitor and an
/// accumulator.
///
/// @details The accumulator's deduced value type is the result type. An empty
/// tuple returns it without invoking `visit`. C++17 and newer assign each
/// visitor result to a named accumulator, avoiding template recursion. C++11
/// and C++14 instead use expression-only recursion because their
/// constant-expression rules reject that mutation; each result directly
/// initializes the next accumulator.
constexpr Accumulator
  foldl(Visit visit, Accumulator accumulator, Tuple &&tuple) {
#if defined(__cpp_constexpr) && 201603L <= __cpp_constexpr \
  && defined(__cpp_generic_lambdas)
  return compat::apply(
    [&visit, &accumulator](auto &&...element) constexpr -> Accumulator {
      const int folded[] = {
        0,
        ((accumulator = compat::invoke(visit,
            std::move(accumulator),
            std::forward<decltype(element)>(element))),
          0)...,
      };
      static_cast<void>(folded);
      return std::move(accumulator);
    },
    std::forward<Tuple>(tuple));
#else
  return compat::apply(
    detail::cpp11_foldl_visit<Visit, Accumulator>{visit, accumulator},
    std::forward<Tuple>(tuple));
#endif
}

#if defined(__cpp_concepts) && 201907L <= __cpp_concepts
template <typename Visit, typename Accumulator, detail::tuple_like Tuple>
#else
template <typename Visit,
  typename Accumulator,
  typename Tuple,
  typename std::enable_if<
    detail::is_tuple_like<compat::remove_cvref_t<Tuple>>::value,
    int>::type = 0>
#endif
/// Right-fold a tuple with local copies or moves of a binary visitor and an
/// accumulator.
///
/// @details The accumulator's deduced value type is the result type. An empty
/// tuple returns it without invoking `visit`. Elements are visited
/// right-to-left; each visitor result directly initializes the accumulator
/// used with the preceding element.
constexpr Accumulator
  foldr(Visit visit, Accumulator accumulator, Tuple &&tuple) {
  return compat::apply(
    detail::foldr_visit<Visit, Accumulator>{visit, accumulator},
    std::forward<Tuple>(tuple));
}

/// Stores a conversion for deferred application.
template <typename Conversion>
struct as_closure {
  Conversion conversion;

  template <typename Value>
  constexpr auto operator()(Value &&value) const & -> decltype(as(conversion,
    std::forward<Value>(value))) {
    return as(conversion, std::forward<Value>(value));
  }

  // C++14 removed implicit const from constexpr member functions.
  template <typename Value>
#if 201402L <= OMNI_CPLUSPLUS
  constexpr
#endif
    auto operator()(Value &&value) && -> decltype(as(std::move(conversion),
      std::forward<Value>(value))) {
    return as(std::move(conversion), std::forward<Value>(value));
  }

  template <typename Value>
  friend constexpr auto operator|(Value &&value, as_closure closure)
    -> decltype(as(std::declval<Conversion &&>(), std::forward<Value>(value))) {
    return as(std::move(closure.conversion), std::forward<Value>(value));
  }
};

/// Stores a tuple for deferred concatenation.
template <typename Right>
struct concat_closure {
  Right right;

  template <typename Left>
  constexpr auto operator()(
    Left &&left) const & -> decltype(concat(std::forward<Left>(left), right)) {
    return concat(std::forward<Left>(left), right);
  }

  // C++14 removed implicit const from constexpr member functions.
  template <typename Left>
#if 201402L <= OMNI_CPLUSPLUS
  constexpr
#endif
    auto operator()(Left &&left) && -> decltype(concat(std::forward<Left>(left),
      std::move(right))) {
    return concat(std::forward<Left>(left), std::move(right));
  }

  template <typename Left>
  friend constexpr auto operator|(Left &&left, concat_closure closure)
    -> decltype(concat(std::forward<Left>(left), std::declval<Right &&>())) {
    return concat(std::forward<Left>(left), std::move(closure.right));
  }
};

/// Stores a predicate for deferred `any_of` application.
template <typename Predicate>
struct any_of_closure {
  Predicate predicate;

  template <typename Tuple>
  constexpr auto operator()(Tuple &&tuple) const & -> decltype(any_of(predicate,
    std::forward<Tuple>(tuple))) {
    return any_of(predicate, std::forward<Tuple>(tuple));
  }

  // C++14 removed implicit const from constexpr member functions.
  template <typename Tuple>
#if 201402L <= OMNI_CPLUSPLUS
  constexpr
#endif
    auto operator()(Tuple &&tuple) && -> decltype(any_of(std::move(predicate),
      std::forward<Tuple>(tuple))) {
    return any_of(std::move(predicate), std::forward<Tuple>(tuple));
  }

  template <typename Tuple>
  friend constexpr auto operator|(Tuple &&tuple, any_of_closure closure)
    -> decltype(any_of(std::declval<Predicate &&>(),
      std::forward<Tuple>(tuple))) {
    return any_of(std::move(closure.predicate), std::forward<Tuple>(tuple));
  }
};

/// Stores a predicate for deferred `none_of` application.
template <typename Predicate>
struct none_of_closure {
  Predicate predicate;

  template <typename Tuple>
  constexpr auto operator()(
    Tuple &&tuple) const & -> decltype(none_of(predicate,
    std::forward<Tuple>(tuple))) {
    return none_of(predicate, std::forward<Tuple>(tuple));
  }

  // C++14 removed implicit const from constexpr member functions.
  template <typename Tuple>
#if 201402L <= OMNI_CPLUSPLUS
  constexpr
#endif
    auto operator()(Tuple &&tuple) && -> decltype(none_of(std::move(predicate),
      std::forward<Tuple>(tuple))) {
    return none_of(std::move(predicate), std::forward<Tuple>(tuple));
  }

  template <typename Tuple>
  friend constexpr auto operator|(Tuple &&tuple, none_of_closure closure)
    -> decltype(none_of(std::declval<Predicate &&>(),
      std::forward<Tuple>(tuple))) {
    return none_of(std::move(closure.predicate), std::forward<Tuple>(tuple));
  }
};

/// Stores a predicate for deferred negation.
template <typename Predicate>
struct not_closure {
  Predicate predicate;

  template <typename... Element>
  constexpr bool operator()() const {
    return !predicate.template operator()<Element...>();
  }

  template <typename First, typename... Argument>
  constexpr auto operator()(First &&first, Argument &&...argument)
    const & -> decltype(not_(std::declval<const Predicate &>(),
      std::forward<First>(first),
      std::forward<Argument>(argument)...)) {
    return not_(predicate,
      std::forward<First>(first),
      std::forward<Argument>(argument)...);
  }

  // C++14 removed implicit const from constexpr member functions.
  template <typename First, typename... Argument>
#if 201402L <= OMNI_CPLUSPLUS
  constexpr
#endif
    auto operator()(First &&first,
      Argument &&...argument) && -> decltype(not_(std::declval<Predicate &&>(),
      std::forward<First>(first),
      std::forward<Argument>(argument)...)) {
    return not_(std::move(predicate),
      std::forward<First>(first),
      std::forward<Argument>(argument)...);
  }

  template <typename Value>
  friend constexpr auto operator|(Value &&value, not_closure closure)
    -> decltype(not_(std::declval<Predicate &&>(),
      std::forward<Value>(value))) {
    return not_(std::move(closure.predicate), std::forward<Value>(value));
  }
};

/// Stores a key projection and right tuple for deferred `diff_by`.
template <typename Key, typename Right>
struct diff_by_closure {
  Key key;
  Right right;

  template <typename Left>
  constexpr auto operator()(Left &&left) const & -> decltype(diff_by(key,
    std::forward<Left>(left),
    right)) {
    return diff_by(key, std::forward<Left>(left), right);
  }

  // C++14 removed implicit const from constexpr member functions.
  template <typename Left>
#if 201402L <= OMNI_CPLUSPLUS
  constexpr
#endif
    auto operator()(Left &&left) && -> decltype(diff_by(std::move(key),
      std::forward<Left>(left),
      std::move(right))) {
    return diff_by(std::move(key), std::forward<Left>(left), std::move(right));
  }

  template <typename Left>
  friend constexpr auto operator|(Left &&left, diff_by_closure closure)
    -> decltype(diff_by(std::declval<Key &&>(),
      std::forward<Left>(left),
      std::declval<Right &&>())) {
    return diff_by(std::move(closure.key),
      std::forward<Left>(left),
      std::move(closure.right));
  }
};

/// Stores a visitor for deferred `each` application.
template <typename Visit>
struct each_closure {
  Visit visit;

  template <typename Tuple>
  constexpr auto operator()(Tuple &&tuple) const & -> decltype(each(visit,
    std::forward<Tuple>(tuple))) {
    return each(visit, std::forward<Tuple>(tuple));
  }

  // C++14 removed implicit const from constexpr member functions.
  template <typename Tuple>
#if 201402L <= OMNI_CPLUSPLUS
  constexpr
#endif
    auto operator()(Tuple &&tuple) && -> decltype(each(std::move(visit),
      std::forward<Tuple>(tuple))) {
    return each(std::move(visit), std::forward<Tuple>(tuple));
  }

  template <typename Tuple>
  friend constexpr auto operator|(Tuple &&tuple, each_closure closure)
    -> decltype(each(std::declval<Visit &&>(), std::forward<Tuple>(tuple))) {
    return each(std::move(closure.visit), std::forward<Tuple>(tuple));
  }
};

/// Stores a type predicate for deferred `filter` application.
template <typename Predicate>
struct filter_closure {
  Predicate predicate;

  template <typename Tuple>
  constexpr auto operator()(Tuple &&tuple) const & -> decltype(filter(predicate,
    std::forward<Tuple>(tuple))) {
    return filter(predicate, std::forward<Tuple>(tuple));
  }

  // C++14 removed implicit const from constexpr member functions.
  template <typename Tuple>
#if 201402L <= OMNI_CPLUSPLUS
  constexpr
#endif
    auto operator()(Tuple &&tuple) && -> decltype(filter(std::move(predicate),
      std::forward<Tuple>(tuple))) {
    return filter(std::move(predicate), std::forward<Tuple>(tuple));
  }

  template <typename Tuple>
  friend constexpr auto operator|(Tuple &&tuple, filter_closure closure)
    -> decltype(filter(std::declval<Predicate &&>(),
      std::forward<Tuple>(tuple))) {
    return filter(std::move(closure.predicate), std::forward<Tuple>(tuple));
  }
};

/// Stores a visitor for deferred `map` application.
template <typename Visit>
struct map_closure {
  Visit visit;

  template <typename Tuple>
  constexpr auto operator()(
    Tuple &&tuple) const & -> decltype(map(visit, std::forward<Tuple>(tuple))) {
    return map(visit, std::forward<Tuple>(tuple));
  }

  // C++14 removed implicit const from constexpr member functions.
  template <typename Tuple>
#if 201402L <= OMNI_CPLUSPLUS
  constexpr
#endif
    auto operator()(Tuple &&tuple) && -> decltype(map(std::move(visit),
      std::forward<Tuple>(tuple))) {
    return map(std::move(visit), std::forward<Tuple>(tuple));
  }

  template <typename Tuple>
  friend constexpr auto operator|(Tuple &&tuple, map_closure closure)
    -> decltype(map(std::declval<Visit &&>(), std::forward<Tuple>(tuple))) {
    return map(std::move(closure.visit), std::forward<Tuple>(tuple));
  }
};

/// Stores a visitor and accumulator for deferred `foldl` application.
template <typename Visit, typename Accumulator>
struct foldl_closure {
  Visit visit;
  Accumulator accumulator;

  template <typename Tuple>
  constexpr auto operator()(Tuple &&tuple) const & -> decltype(foldl(visit,
    accumulator,
    std::forward<Tuple>(tuple))) {
    return foldl(visit, accumulator, std::forward<Tuple>(tuple));
  }

  // C++14 removed implicit const from constexpr member functions.
  template <typename Tuple>
#if 201402L <= OMNI_CPLUSPLUS
  constexpr
#endif
    auto operator()(Tuple &&tuple) && -> decltype(foldl(std::move(visit),
      std::move(accumulator),
      std::forward<Tuple>(tuple))) {
    return foldl(std::move(visit),
      std::move(accumulator),
      std::forward<Tuple>(tuple));
  }

  template <typename Tuple>
  friend constexpr auto operator|(Tuple &&tuple, foldl_closure closure)
    -> decltype(foldl(std::declval<Visit &&>(),
      std::declval<Accumulator &&>(),
      std::forward<Tuple>(tuple))) {
    return foldl(std::move(closure.visit),
      std::move(closure.accumulator),
      std::forward<Tuple>(tuple));
  }
};

/// Stores a visitor and accumulator for deferred `foldr` application.
template <typename Visit, typename Accumulator>
struct foldr_closure {
  Visit visit;
  Accumulator accumulator;

  template <typename Tuple>
  constexpr auto operator()(Tuple &&tuple) const & -> decltype(foldr(visit,
    accumulator,
    std::forward<Tuple>(tuple))) {
    return foldr(visit, accumulator, std::forward<Tuple>(tuple));
  }

  // C++14 removed implicit const from constexpr member functions.
  template <typename Tuple>
#if 201402L <= OMNI_CPLUSPLUS
  constexpr
#endif
    auto operator()(Tuple &&tuple) && -> decltype(foldr(std::move(visit),
      std::move(accumulator),
      std::forward<Tuple>(tuple))) {
    return foldr(std::move(visit),
      std::move(accumulator),
      std::forward<Tuple>(tuple));
  }

  template <typename Tuple>
  friend constexpr auto operator|(Tuple &&tuple, foldr_closure closure)
    -> decltype(foldr(std::declval<Visit &&>(),
      std::declval<Accumulator &&>(),
      std::forward<Tuple>(tuple))) {
    return foldr(std::move(closure.visit),
      std::move(closure.accumulator),
      std::forward<Tuple>(tuple));
  }
};

/// Store `conversion` for later call or pipe application.
template <typename Conversion>
constexpr as_closure<Conversion> as(Conversion conversion) {
  return as_closure<Conversion>{std::move(conversion)};
}

/// Store construction of an explicitly selected type for later application.
template <typename To>
constexpr as_closure<type_t<To>> as() {
  return as(type_t<To>{});
}

/// Store CTAD or its value-type fallback for later application.
template <template <typename...> class Template>
constexpr auto as() -> as_closure<decltype(ctad<Template>())> {
  return as(ctad<Template>());
}

#if defined(__cpp_deduction_guides) && 201703L <= __cpp_deduction_guides
/// Store type-and-size class-template deduction for later application.
template <template <typename, std::size_t> class Template>
constexpr auto as() -> as_closure<decltype(ctad<Template>())> {
  return as(ctad<Template>());
}
#endif

/// Store `visit` for later call or pipe application to a tuple-like object.
template <typename Visit>
constexpr each_closure<Visit> each(Visit visit) {
  return each_closure<Visit>{std::move(visit)};
}

/// Store `predicate` for later call or pipe application to a tuple-like object.
template <typename Predicate>
constexpr filter_closure<Predicate> filter(Predicate predicate) {
  return filter_closure<Predicate>{std::move(predicate)};
}

/// Store `visit` for later call or pipe application to a tuple-like object.
template <typename Visit>
constexpr map_closure<Visit> map(Visit visit) {
  return map_closure<Visit>{std::move(visit)};
}

/// Store `visit` and `accumulator` for later left-fold application.
template <typename Visit, typename Accumulator>
constexpr foldl_closure<Visit, Accumulator> foldl(Visit visit,
  Accumulator accumulator) {
  return foldl_closure<Visit, Accumulator>{
    std::move(visit),
    std::move(accumulator),
  };
}

/// Store `visit` and `accumulator` for later right-fold application.
template <typename Visit, typename Accumulator>
constexpr foldr_closure<Visit, Accumulator> foldr(Visit visit,
  Accumulator accumulator) {
  return foldr_closure<Visit, Accumulator>{
    std::move(visit),
    std::move(accumulator),
  };
}

/// Store `right` for later concatenation with a tuple-like object.
template <typename Right>
constexpr concat_closure<compat::decay_t<Right>> concat(Right &&right) {
  return concat_closure<compat::decay_t<Right>>{
    std::forward<Right>(right),
  };
}

/// Store `predicate` for later `any_of` application.
template <typename Predicate>
constexpr any_of_closure<Predicate> any_of(Predicate predicate) {
  return any_of_closure<Predicate>{std::move(predicate)};
}

/// Store `predicate` for later `none_of` application.
template <typename Predicate>
constexpr none_of_closure<Predicate> none_of(Predicate predicate) {
  return none_of_closure<Predicate>{std::move(predicate)};
}

/// Store `predicate` for later negated call or type-predicate application.
template <typename Predicate>
constexpr not_closure<Predicate> not_(Predicate predicate) {
  return not_closure<Predicate>{std::move(predicate)};
}

/// Store `key` and `right` for later tuple difference application.
template <typename Key, typename Right>
constexpr diff_by_closure<Key, compat::decay_t<Right>> diff_by(Key key,
  Right &&right) {
  return diff_by_closure<Key, compat::decay_t<Right>>{
    std::move(key),
    std::forward<Right>(right),
  };
}

} // namespace fn
} // namespace omni
