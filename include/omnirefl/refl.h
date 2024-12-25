#pragma once

// todo: copiright notice (MIT)

#if __cplusplus >= 201703L
#  define OMNI_HAS_CPP17
#  define OMNI_CONSTEXPR constexpr
#else
#  define OMNI_CONSTEXPR
#endif

#include <type_traits>
#include <utility>

#ifdef OMNI_HAS_CPP17
#  include <mpark/variant.hpp>
#  include <nonstd/string_view.hpp>
namespace omni {
using mpark::variant;
using mpark::visit;

using nonstd::string_view;
} // namespace omni
#else
#  include <mpark/variant.hpp>
#  include <nonstd/string_view.hpp>
namespace omni {
using mpark::variant;
using mpark::visit;

using nonstd::string_view;
} // namespace omni
#endif

// todo: iteration 2
//  - provide `omni` utilities
//  - should be compatible with at least C++11
//  - must only define reflection-related interface
namespace omni {
namespace detail {
namespace {
template <typename... Ts>
struct make_void {
  typedef void type;
};

template <typename... Ts>
using void_t = typename make_void<Ts...>::type;
} // namespace
} // namespace detail

template <typename T>
struct reflected_t;

/// (!!!) do not instantiate this outside reflected context
template <typename, typename = void>
struct is_reflected: std::false_type {};

// fixme: this will not work with existing definition for `reflected_t` that is used for exposition
/// (!!!) do not instantiate this outside reflected context
template <typename T>
struct is_reflected<T, detail::void_t<decltype(sizeof(reflected_t<T>))>>: std::true_type {};

// todo: remind and explain myself why do I need this
// todo: this should be an exposition-only interface that should never be instantiated
// template <typename T>
// struct reflected_t {
//   static_assert(((T *)nullptr, false), "reflected_t<T> has not been specialized!");
//   using type = T;
//
// #ifndef OMNI_DEFINE_NAME_FUNC
// #  define OMNI_DEFINE_NAME_FUNC(STR) \
//     constexpr static auto name() noexcept -> const char(&)[sizeof(STR)] { \
//       return STR; \
//     }
// #endif
//
//   struct field_a_t {
//     OMNI_DEFINE_NAME_FUNC("field_a");
//     constexpr static int get_value(const type &t) noexcept {
//       return t.field_a;
//     }
//
//     template <typename Value>
//     static void set_value(type &t, Value &&v) noexcept {
//       t.field_a = std::forward<Value>(v);
//     }
//   } constexpr static field_a{};
//
//   struct field_b_t {
//     OMNI_DEFINE_NAME_FUNC("field_b");
//     constexpr static const std::string &get_value(const type &t) noexcept {
//       return t.field_b;
//     }
//
//     template <typename Value>
//     static void set_value(type &t, Value &&v) noexcept {
//       t.field_b = std::forward<Value>(v);
//     }
//   } constexpr static field_b{};
//
//   using fields_t = std::tuple<field_a_t, field_b_t>;
// #undef OMNI_DEFINE_NAME_FUNC
//
//   auto operator()(T &) const noexcept {
//     // todo: return some kind of binding for easy access to the actual fields values
//   }
// };

/// access reflection data using a (potentially const) reference to the reflected object.
/// `reflected_t<T>` must be specialized at the point of invocation
template <typename T>
OMNI_CONSTEXPR auto reflected(T &t) noexcept {
  return reflected_t<typename std::decay<T>::type>{}(t);
}

namespace detail {
namespace {
// tag used to collect reflected types
template <typename>
struct _reflected_type {};

// tag used to collect reflected implementation types
template <typename>
struct _reflected_impl {};
} // namespace
} // namespace detail

/// meta function to register the type for reflection
template <typename T>
OMNI_CONSTEXPR void reflect(const T &) {
  (void)detail::_reflected_type<typename std::decay<T>::type>{};
}

/// meta function to register the type for reflection
template <typename T>
OMNI_CONSTEXPR void reflect() {
  (void)detail::_reflected_type<typename std::decay<T>::type>{};
}

/// meta function to register the type to be used as implementation
template <typename T>
OMNI_CONSTEXPR void use_impl(const T &) {
  (void)detail::_reflected_impl<typename std::decay<T>::type>{};
}

/// meta function to register the type to be used as implementation
template <typename T>
OMNI_CONSTEXPR void use_impl() {
  (void)detail::_reflected_impl<typename std::decay<T>::type>{};
}

/// class to invoke a callable implementation object
struct reflected_call_t {
  template <typename Impl, typename T>
  void operator()(Impl &&impl, T &&t) const {
    use_impl(impl);
    reflect(t);
    _call_impl(std::forward<Impl>(impl), std::forward<T>(t));
  }

  template <typename Impl, typename T, typename R>
  void operator()(Impl &&impl, T &&t, R &result) const {
    use_impl(impl);
    reflect(t);
    _call_impl(std::forward<Impl>(impl), std::forward<T>(t), result);
  }

  private:
  // implementation will be generated for this function by omnirefl
  template <typename Impl, typename... Args>
  static void _call_impl(Impl &&impl, Args &&...args);
} const reflected_call{};

} // namespace omni
