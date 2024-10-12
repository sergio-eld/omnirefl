#pragma once

// todo: copiright notice (MIT)

#include <array>
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
//  - remove default implementation
//  - provide `omni` utilities
//  - should be compatible with at least C++11
namespace omni {
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
} const inline reflected_call;

template <typename>
struct reflected_t;

template <typename T>
OMNI_CONSTEXPR auto reflected(T &t) noexcept {
  return reflected_t<typename std::decay<T>::type>{}(t);
}

namespace detail {
namespace {

template <typename T, typename... Ts>
struct filter_duplicates {
  using type = T;
};

template <template <typename...> class C, typename... Ts, typename U, typename... Us>
struct filter_duplicates<C<Ts...>, U, Us...>:
    std::conditional<(std::is_same<U, Ts>::value || ...),
      filter_duplicates<C<Ts...>, Us...>,
      filter_duplicates<C<Ts..., U>, Us...>>::type {};

template <typename... Ts>
using unique_variant = typename filter_duplicates<omni::variant<>, Ts...>::type;

// internal utility to specialize reflected types' metadata
template <typename>
struct fields_metadata;

template <typename T, typename /*fields_type_list*/ = decltype(fields_metadata<T>{}())>
struct make_reflected_t;

// utility to facilitate code generation
template <template <typename...> class TypeList, typename T, typename... MetaFieldData>
struct make_reflected_t<T, TypeList<MetaFieldData...>> {
  // std::variant doesn't allow for reference types
  // fixme: need a static assert that getter returns const reference and not by value
  using common_field_cref_t = unique_variant<std::reference_wrapper<
    std::remove_reference_t<decltype(MetaFieldData::getter()(std::declval<T>()))>>...>;

  // run-time wrapper for field's value const reference
  struct _rt_value_cref {
    common_field_cref_t value;

    template <typename C>
    decltype(auto) match(C c) const {
      return omni::visit([&c](const auto &cref) { return c(cref.get()); }, value);
    }

    // todo: else, else_if
    template <template <typename, typename...> class Pred, typename... PredArgs, typename C>
    auto match_if(C c) const {
      omni::visit(
        [&c](const auto &cref) {
          // todo: make C++11 friendly
          if constexpr (Pred<std::decay_t<decltype(cref.get())>, PredArgs...>::value) {
            c(cref.get());
          } else {
            void(); // noop
          }
        },
        value);
    }
  };

  // run-time interface for accessing a field of the reflected type
  struct _rt_field_cref {
    omni::string_view name;
    _rt_value_cref value;
  };

  // run-time interface for iterating over fields of the reflected type
  struct _cref {
    const T &ref;
    std::array<_rt_field_cref, sizeof...(MetaFieldData)> fields;
  };

  _cref operator()(const T &t) const noexcept {
    return {t, {_rt_field_cref{MetaFieldData::name(), MetaFieldData::getter()(t)}...}};
  };

  // todo: `apply` interface for tuple-like access
};
} // namespace
} // namespace detail
} // namespace omni

// todo: remove in iteration 2
// default implementation for `iteration 1`
#include <ryml/ryml.hpp>
#include <tl/expected.hpp>

namespace omni {
namespace detail {
template <typename... Ts>
struct make_void {
  typedef void type;
};

template <typename... Ts>
using void_t = typename make_void<Ts...>::type;

struct _noop {};
// todo: second arg
void serialize(_noop, _noop);
void deserialize(_noop, _noop);

template <typename, typename = void>
struct serialize_implemented_by_user: std::false_type {};

template <typename T>
struct serialize_implemented_by_user<T, void_t<decltype(serialize(std::declval<const T>()))>>:
    std::true_type {};

template <typename, typename, typename = void>
struct deserialize_implemented_by_user: std::false_type {};

template <typename Data, typename T>
struct deserialize_implemented_by_user<Data,
  T,
  void_t<decltype(deserialize(std::declval<const Data>(), std::declval<T &>()))>>:
    std::true_type {};
} // namespace detail

// todo: implement with ryml
// class to serialize a Type to a data format
class serialize_t {
  public:
  // todo:
  // - return type configuration. Should be `std::expected` when available,
  //   `tl::expected` otherwise. Should be configured outside the scope of this
  //   header
  // - second argument to be used to detect the output format and other
  //   configurable stuff. User should have an understandable customization
  //   mechanism and the ability to provide his own implementation with/or
  //   partial overloads
  template <typename T>
  decltype(auto) operator()(const T & v) const {
    if constexpr (detail::serialize_implemented_by_user<std::decay_t<T>>()) {
      using detail::serialize;
      return serialize(v);
    } else {
      // todo: implement support for anonymous, local and in-cpp struct
      // definitions
      std::string to;
      return _impl(v, to), to;
    }
  }

  private:
  // implementation will be generated for this function by omnirefl
  template <typename T>
  static void _impl(const T &, std::string &);

} constexpr const inline serialize{};

// class to deserialize a data format to a Type
class deserialize_t {
  public:
  // todo:
  // - return type configuration. Should be `std::expected` when available,
  //   `tl::expected` otherwise. Should be configured outside the scope of this
  //   header
  // - second argument to be used to detect the output format and other
  //   configurable stuff. User should have an understandable customization
  //   mechanism and the ability to provide his own implementation with/or
  //   partial overloads
  template <typename T>
  decltype(auto) operator()(const ryml::ConstNodeRef & data, T & to) const {
    if constexpr (detail::deserialize_implemented_by_user<ryml::ConstNodeRef, std::decay_t<T>>()) {
      using detail::deserialize;
      return deserialize(data, to);
    } else {
      // todo: implement support for anonymous, local and in-cpp struct
      // definitions
      return _impl(data, to);
    }
  }

  // todo: support generic return type (from user-defined implementation)
  template <typename T>
  tl::expected<T, std::string> to(const ryml::ConstNodeRef &data) const {
    tl::expected<T, std::string> to{tl::in_place};
    auto res = (*this)(data, *to);
    if (res)
      return to;
    return tl::make_unexpected(std::move(res).error());
  }

  private:
  // implementation will be generated for this function by omnirefl
  template <typename T>
  static tl::expected<void, std::string> _impl(const ryml::ConstNodeRef &, T &);

} constexpr const inline deserialize{};

} // namespace omni
