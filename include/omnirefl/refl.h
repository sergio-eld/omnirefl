#pragma once

// todo: copiright notice (MIT)

// default implementation for `iteration 1`
#include <ryml/ryml.hpp>
#include <tl/expected.hpp>

#include <type_traits>

namespace omni {
namespace detail {
template <typename... Ts>
struct make_void {
  typedef void type;
};

template <typename... Ts>
using void_t = typename make_void<Ts...>::type;

struct _noop;
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
