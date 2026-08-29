#pragma once

#include <omnirefl/compat.hpp>

#include <tuple>
#include <type_traits>
#include <utility>

// Clang 22 accepts captureless lambdas as non-type template arguments but
// still reports __cpp_nontype_template_args == 201411L.
#if defined(__cpp_generic_lambdas) \
  && 201707L <= __cpp_generic_lambdas \
  && 202002L <= OMNI_CPLUSPLUS
#  define OMNI_FN_HAS_PREDICATE_LAMBDA 1
#else
#  define OMNI_FN_HAS_PREDICATE_LAMBDA 0
#endif

namespace omni {
namespace fn {
namespace detail {

template <typename Tuple, typename = void>
struct is_tuple_like: std::false_type {};

template <typename Tuple>
struct is_tuple_like<Tuple,
  compat::void_t<decltype(std::tuple_size<Tuple>::value)>>: std::true_type {};

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
      compat::conditional_t<
        Predicate{}.template operator()<decltype(
          std::get<Index>(std::declval<Tuple>()))>(),
        compat::index_sequence<Selected..., Index>,
        compat::index_sequence<Selected...>>> {};

template <typename Predicate,
  typename Tuple,
  std::size_t... Selected>
struct filtered_indices<Predicate,
  Tuple,
  compat::index_sequence<>,
  compat::index_sequence<Selected...>> {
  static_assert(std::is_empty<Predicate>::value,
    "filter predicates must encode selection in their type");
  static_assert(std::is_default_constructible<Predicate>::value,
    "filter predicates must be default constructible");

  using type = compat::index_sequence<Selected...>;
};

template <typename Tuple, std::size_t... Index>
constexpr auto select_tuple(Tuple &&tuple, compat::index_sequence<Index...>)
  -> std::tuple<compat::decay_t<decltype(
    std::get<Index>(std::forward<Tuple>(tuple)))>...> {
  return {std::get<Index>(std::forward<Tuple>(tuple))...};
}

template <typename Predicate, typename Tuple>
constexpr auto filter_values(Tuple &&tuple)
  -> decltype(select_tuple(std::forward<Tuple>(tuple),
    typename filtered_indices<Predicate,
      Tuple &&,
      compat::make_index_sequence<std::tuple_size<
        compat::remove_cvref_t<Tuple>>::value>>::type{})) {
  return select_tuple(std::forward<Tuple>(tuple),
    typename filtered_indices<Predicate,
      Tuple &&,
      compat::make_index_sequence<std::tuple_size<
        compat::remove_cvref_t<Tuple>>::value>>::type{});
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
  -> decltype(detail::filter_values<Predicate>(
    std::forward<Tuple>(tuple))) {
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
constexpr Accumulator foldl(Visit visit,
  Accumulator accumulator,
  Tuple &&tuple) {
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
constexpr Accumulator foldr(Visit visit,
  Accumulator accumulator,
  Tuple &&tuple) {
  return compat::apply(
    detail::foldr_visit<Visit, Accumulator>{visit, accumulator},
    std::forward<Tuple>(tuple));
}

/// Stores a visitor for deferred `each` application.
template <typename Visit>
struct each_closure {
  Visit visit;

  template <typename Tuple>
  constexpr auto operator()(Tuple &&tuple) const &
    -> decltype(each(visit, std::forward<Tuple>(tuple))) {
    return each(visit, std::forward<Tuple>(tuple));
  }

  // C++14 removed implicit const from constexpr member functions.
  template <typename Tuple>
#if 201402L <= OMNI_CPLUSPLUS
  constexpr
#endif
    auto operator()(Tuple &&tuple) &&
      -> decltype(each(std::move(visit), std::forward<Tuple>(tuple))) {
    return each(std::move(visit), std::forward<Tuple>(tuple));
  }

  template <typename Tuple>
  friend constexpr auto operator|(Tuple &&tuple, each_closure closure)
    -> decltype(each(std::declval<Visit &&>(),
      std::forward<Tuple>(tuple))) {
    return each(std::move(closure.visit), std::forward<Tuple>(tuple));
  }
};

/// Stores a type predicate for deferred `filter` application.
template <typename Predicate>
struct filter_closure {
  Predicate predicate;

  template <typename Tuple>
  constexpr auto operator()(Tuple &&tuple) const &
    -> decltype(filter(predicate, std::forward<Tuple>(tuple))) {
    return filter(predicate, std::forward<Tuple>(tuple));
  }

  // C++14 removed implicit const from constexpr member functions.
  template <typename Tuple>
#if 201402L <= OMNI_CPLUSPLUS
  constexpr
#endif
    auto operator()(Tuple &&tuple) &&
      -> decltype(filter(std::move(predicate),
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
  constexpr auto operator()(Tuple &&tuple) const &
    -> decltype(map(visit, std::forward<Tuple>(tuple))) {
    return map(visit, std::forward<Tuple>(tuple));
  }

  // C++14 removed implicit const from constexpr member functions.
  template <typename Tuple>
#if 201402L <= OMNI_CPLUSPLUS
  constexpr
#endif
    auto operator()(Tuple &&tuple) &&
      -> decltype(map(std::move(visit), std::forward<Tuple>(tuple))) {
    return map(std::move(visit), std::forward<Tuple>(tuple));
  }

  template <typename Tuple>
  friend constexpr auto operator|(Tuple &&tuple, map_closure closure)
    -> decltype(map(std::declval<Visit &&>(),
      std::forward<Tuple>(tuple))) {
    return map(std::move(closure.visit), std::forward<Tuple>(tuple));
  }
};

/// Stores a visitor and accumulator for deferred `foldl` application.
template <typename Visit, typename Accumulator>
struct foldl_closure {
  Visit visit;
  Accumulator accumulator;

  template <typename Tuple>
  constexpr auto operator()(Tuple &&tuple) const &
    -> decltype(foldl(visit, accumulator, std::forward<Tuple>(tuple))) {
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
  constexpr auto operator()(Tuple &&tuple) const &
    -> decltype(foldr(visit, accumulator, std::forward<Tuple>(tuple))) {
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

} // namespace fn
} // namespace omni
