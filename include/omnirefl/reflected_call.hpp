#pragma once

// todo: copyright and detailed explanation
//
// this header adds support for reflecting types via reflected_call

#include <omnirefl/compat.hpp>

#include <type_traits>
#include <utility>

namespace omni {
namespace detail {
namespace {

template <int Id>
struct counter {
  struct generator {
#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wnon-template-friend"
#elif defined _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4396)
#endif

    // This does not compile on GCC < 11, and gives warning if not a template
    // template <typename...>
    friend constexpr bool generate(counter) {
      return true;
    }
  };

  // template <typename...>
  friend constexpr bool generate(counter);

#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER)
#  pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#  pragma warning(pop)
#endif

#if defined _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4514)
#  pragma warning(disable : 4710)
#endif

  template <typename Tag = counter, int I = (int)generate(Tag{})>
  static constexpr std::true_type exists(int) {
    return {};
  }

  static constexpr std::false_type exists(...) {
    return generator(), std::false_type{};
  }
#if defined(_MSC_VER)
#  pragma warning(pop)
#endif
};

template <typename T, int Id>
constexpr int unique_id(std::false_type) {
  return Id;
}

template <typename T, int Id>
constexpr int unique_id(std::true_type);

template <typename T, int Id = int{}>
constexpr int unique_id() {
  return unique_id<T, Id>(counter<Id>::exists(Id));
}

template <typename T, int Id>
constexpr int unique_id(std::true_type) {
  return unique_id<T, Id + 1>();
}

template <int Index, typename T>
struct reflected_index_match {
  struct generator {
#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wnon-template-friend"
#elif defined _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4396)
#endif

    friend constexpr bool reflected_index_found(reflected_index_match) {
      return true;
    }

#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER)
#  pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#  pragma warning(pop)
#endif
  };

#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wnon-template-friend"
#  pragma GCC diagnostic ignored "-Wunused-function"
#elif defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wundefined-internal"
#elif defined _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4396)
#endif

  friend constexpr bool reflected_index_found(reflected_index_match);

#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER)
#  pragma GCC diagnostic pop
#elif defined(__clang__)
#  pragma clang diagnostic pop
#elif defined(_MSC_VER)
#  pragma warning(pop)
#endif

  template <typename Tag = reflected_index_match,
    bool = (bool)reflected_index_found(Tag{})>
  static constexpr std::true_type exists(int) {
    return {};
  }

  static constexpr std::false_type exists(...) {
    return {};
  }
};

// tag used to assign index to the type upon instantiation
template <typename T, int Index = unique_id<T>()>
struct _reflected_indexed_type {
  typename reflected_index_match<Index, T>::generator _{};
};

} // namespace
} // namespace detail

/// class to invoke a callable implementation object
struct reflected_call_t {
  template <typename Impl, typename T, typename... Args>
  auto operator()(Impl &&impl, T &&t, Args &&...args) const
    -> decltype(std::declval<Impl &&>()(std::declval<T &&>(),
      std::declval<Args &&>()...)) {
#if defined(OMNI_ENABLE_INDEX_MODE) && OMNI_ENABLE_INDEX_MODE
    using type = typename std::decay<T>::type;
    (void)detail::_reflected_indexed_type<type>{};
#endif

    // todo: suppress missing return warning for tool invocation

    // ad hoc to prevent "compilation errors" during the omnirefl run
#ifdef OMNI_INCLUDED_GENERATED_REFLECTION_HEADER
    return std::forward<Impl>(impl)(std::forward<T>(t), //
      std::forward<Args>(args)...);
#endif
  }

  template <typename Impl, typename T, typename... Args>
  auto operator()(Impl &&impl,
    compat::type_identity<T> t,
    Args &&...args) const
    -> decltype(std::declval<Impl &&>()(std::declval<compat::type_identity<T>>(),
      std::declval<Args &&>()...)) {
#if defined(OMNI_ENABLE_INDEX_MODE) && OMNI_ENABLE_INDEX_MODE
    (void)detail::_reflected_indexed_type<T>{};
#endif

#ifdef OMNI_INCLUDED_GENERATED_REFLECTION_HEADER
    return std::forward<Impl>(impl)(t, std::forward<Args>(args)...);
#endif
  }

  template <typename Impl, typename... T, typename... Args>
  auto operator()(Impl &&impl, std::tuple<T...> &t, Args &&...args) const
    -> decltype(std::declval<Impl &&>()(std::declval<std::tuple<T...> &>(),
      std::declval<Args &&>()...)) {
#if defined(OMNI_ENABLE_INDEX_MODE) && OMNI_ENABLE_INDEX_MODE
    int dummy[] = {0,
      ((void)detail::_reflected_indexed_type<
         typename std::decay<T>::type>{},
        0)...};
    (void)dummy;
#endif

#ifdef OMNI_INCLUDED_GENERATED_REFLECTION_HEADER
    return std::forward<Impl>(impl)(t, std::forward<Args>(args)...);
#endif
  }

  template <typename Impl, typename... T, typename... Args>
  auto operator()(Impl &&impl, const std::tuple<T...> &t, Args &&...args) const
    -> decltype(std::declval<Impl &&>()(
      std::declval<const std::tuple<T...> &>(),
      std::declval<Args &&>()...)) {
#if defined(OMNI_ENABLE_INDEX_MODE) && OMNI_ENABLE_INDEX_MODE
    int dummy[] = {0,
      ((void)detail::_reflected_indexed_type<
         typename std::decay<T>::type>{},
        0)...};
    (void)dummy;
#endif

#ifdef OMNI_INCLUDED_GENERATED_REFLECTION_HEADER
    return std::forward<Impl>(impl)(t, std::forward<Args>(args)...);
#endif
  }

  template <typename Impl, typename... T, typename... Args>
  auto operator()(Impl &&impl, std::tuple<T...> &&t, Args &&...args) const
    -> decltype(std::declval<Impl &&>()(std::declval<std::tuple<T...> &&>(),
      std::declval<Args &&>()...)) {
#if defined(OMNI_ENABLE_INDEX_MODE) && OMNI_ENABLE_INDEX_MODE
    int dummy[] = {0,
      ((void)detail::_reflected_indexed_type<
         typename std::decay<T>::type>{},
        0)...};
    (void)dummy;
#endif

#ifdef OMNI_INCLUDED_GENERATED_REFLECTION_HEADER
    return std::forward<Impl>(impl)(std::move(t),
      std::forward<Args>(args)...);
#endif
  }
} const reflected_call{};

} // namespace omni
