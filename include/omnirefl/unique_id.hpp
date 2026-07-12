// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Sergei Kolesnik
//
// Friend-injection based indexing used by the experimental indexed reflection
// path. This is instrumentation machinery, not the public reflection interface.
//
// During the real compilation, `reflected_call` can register non-forward
// declarable types observed by the tool. Generated `_reflected<T>` queries must
// only inspect those registrations: probing an unrelated type must not mutate
// reflection state observed by later reflected calls.
//
// Limitation: if a reflected type `T` has member field types that are not
// forward-declarable, those member types are not available for reflection.

#pragma once

#include <type_traits>

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
} // namespace omni
