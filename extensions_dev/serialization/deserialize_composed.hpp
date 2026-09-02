#pragma once

#include "common.hpp"

#include <omnirefl/functional.hpp>

#include <string>
#include <type_traits>
#include <vector>

namespace serialization {
namespace composed {
namespace detail {

template <typename Trait>
struct trait_predicate {
  constexpr bool operator()() const {
    return Trait::value;
  }
};

struct boolean_value {
  ryml::ConstNodeRef from;

  bool operator()(c4::csubstr value) const {
    return serialization::detail::is_boolean(from, value);
  }
};

template <typename To, typename Predicate>
struct parse_value {
  ryml::ConstNodeRef from;
  To *to;
  Predicate predicate;
  c4::csubstr error;

  result<void> operator()() const {
    return serialization::detail::parse_if(from, to, predicate, error);
  }
};

template <typename To>
struct deserialize_fundamental {
  ryml::ConstNodeRef from;
  To *to;

  result<void> operator()() const {
    return omni::fn::cond(
      omni::fn::partial(omni::fn::branch,
        omni::fn::ct_pred(trait_predicate<std::is_same<To, bool>>{}),
        parse_value<To, boolean_value>{from,
          to,
          boolean_value{from},
          c4::to_csubstr(" is not a boolean")}),
      omni::fn::partial(omni::fn::branch,
        omni::fn::ct_pred(trait_predicate<std::integral_constant<bool,
            std::is_integral<To>::value && std::is_unsigned<To>::value>>{}),
        parse_value<To, decltype(&c4::csubstr::is_unsigned_integer)>{from,
          to,
          &c4::csubstr::is_unsigned_integer,
          c4::to_csubstr(" is not an unsigned integer")}),
      omni::fn::partial(omni::fn::branch,
        omni::fn::ct_pred(trait_predicate<std::is_integral<To>>{}),
        parse_value<To, decltype(&c4::csubstr::is_integer)>{from,
          to,
          &c4::csubstr::is_integer,
          c4::to_csubstr(" is not an integer")}),
      omni::fn::partial(omni::fn::branch,
        omni::fn::ct_pred(trait_predicate<std::is_floating_point<To>>{}),
        parse_value<To, decltype(&c4::csubstr::is_real)>{from,
          to,
          &c4::csubstr::is_real,
          c4::to_csubstr(" is not a real number")}),
      unsupported<To>{});
  }

  private:
  template <typename T>
  struct unsupported {
    result<void> operator()() const {
      static_assert(serialization::detail::dependent_false<T>::value,
        "unsupported fundamental type");
      return {};
    }
  };
};

template <typename To>
struct deserialize_string {
  ryml::ConstNodeRef from;
  To *to;

  result<void> operator()() const {
    return serialization::detail::deserialize_string(from, to);
  }
};

template <typename Algorithm, typename To>
struct deserialize_sequence {
  ryml::ConstNodeRef from;
  To *to;

  result<void> operator()() const {
    return serialization::detail::deserialize_sequence<Algorithm>(from, to);
  }
};

template <typename Algorithm, typename To>
struct deserialize_object {
  ryml::ConstNodeRef from;
  To *to;

  result<void> operator()() const {
    return serialization::detail::deserialize_object<Algorithm>(from, to);
  }
};

template <typename Algorithm, typename To>
struct deserialize_reflected {
  ryml::ConstNodeRef from;
  To *to;

  result<void> operator()() const {
    return serialization::detail::deserialize_reflected<Algorithm>(from, to);
  }
};

template <typename To>
struct unsupported {
  result<void> operator()() const {
    static_assert(serialization::detail::dependent_false<To>::value,
      "unsupported deserialization type");
    return {};
  }
};

} // namespace detail

struct algorithm {
  template <typename To>
  static result<void> deserialize(const ryml::ConstNodeRef &from, To *to) {
    using type = omni::compat::remove_cvref_t<To>;
    static_assert(!std::is_pointer<type>::value,
      "destination must not be a pointer");

    return omni::fn::cond(
      omni::fn::partial(omni::fn::branch,
        omni::fn::ct_pred(detail::trait_predicate<std::is_fundamental<type>>{}),
        detail::deserialize_fundamental<type>{from, to}),
      omni::fn::partial(omni::fn::branch,
        omni::fn::ct_pred(detail::trait_predicate<
          serialization::detail::is_specialization<std::basic_string, type>>{}),
        detail::deserialize_string<type>{from, to}),
      omni::fn::partial(omni::fn::branch,
        omni::fn::ct_pred(detail::trait_predicate<
          serialization::detail::is_specialization<std::vector, type>>{}),
        detail::deserialize_sequence<algorithm, type>{from, to}),
      omni::fn::partial(omni::fn::branch,
        omni::fn::ct_pred(
          detail::trait_predicate<serialization::detail::is_specialization<
            serialization::detail::object,
            type>>{}),
        detail::deserialize_object<algorithm, type>{from, to}),
      omni::fn::partial(omni::fn::branch,
        omni::fn::ct_pred(detail::trait_predicate<omni::is_reflected<type>>{}),
        detail::deserialize_reflected<algorithm, type>{from, to}),
      detail::unsupported<type>{});
  }
};

constexpr serialization::detail::deserialize_t<algorithm> deserialize{};

} // namespace composed
} // namespace serialization
