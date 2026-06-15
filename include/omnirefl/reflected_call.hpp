#pragma once

// todo: copyright and detailed explanation
//
// this header adds support for reflecting types via reflected_call

#include <omnirefl/reflected_scope.hpp>

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

template <typename T>
struct reflected_arg_type {
  using type = T;
};

template <typename T>
struct reflected_arg_type<type_t<T>> {
  using type = T;
};

} // namespace
} // namespace detail

/// class to invoke a callable implementation object
template <typename Impl, typename... Args>
auto reflected_call_t::operator()(Impl &&impl, Args &&...args) const
#if defined(OMNI_TOOL_RUN)
  -> decltype(std::declval<Impl &&>()(
    _tool_arg(std::declval<Args &&>())...)) {
#else
  -> decltype(std::declval<Impl &&>()(
    _reflect_arg(std::declval<Args &&>())...)) {
#endif
#if defined(OMNI_ENABLE_INDEX_MODE) && OMNI_ENABLE_INDEX_MODE
  int registered[] = {0,
    ((void)detail::_reflected_indexed_type<
       typename detail::reflected_arg_type<
         typename std::decay<Args>::type>::type>{},
      0)...};
  (void)registered;
#else
  int unused_args[] = {0, ((void)args, 0)...};
  (void)unused_args;
#endif

  (void)impl;

  // todo: suppress missing return warning for tool invocation

  // ad hoc to prevent "compilation errors" during the omnirefl run
#if defined(OMNI_INCLUDED_GENERATED_REFLECTION_HEADER) && !defined(OMNI_TOOL_RUN)
  return std::forward<Impl>(impl)(
    _reflect_arg(std::forward<Args>(args))...);
#endif
}

constexpr reflected_call_t reflected_call{};

} // namespace omni
