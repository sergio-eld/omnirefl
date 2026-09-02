#pragma once

#include "common.hpp"

#include <string>
#include <type_traits>
#include <vector>

namespace serialization {
namespace if_constexpr {

struct algorithm {
  template <typename To>
  static result<void> deserialize_fundamental(const ryml::ConstNodeRef &from,
    To *to) {
    if constexpr (std::is_same<To, bool>::value) {
      return detail::parse_if(
        from,
        to,
        [&from](c4::csubstr value) { return detail::is_boolean(from, value); },
        c4::to_csubstr(" is not a boolean"));
    } else if constexpr (std::is_integral<To>::value
      && std::is_unsigned<To>::value) {
      return detail::parse_if(from,
        to,
        &c4::csubstr::is_unsigned_integer,
        c4::to_csubstr(" is not an unsigned integer"));
    } else if constexpr (std::is_integral<To>::value) {
      return detail::parse_if(from,
        to,
        &c4::csubstr::is_integer,
        c4::to_csubstr(" is not an integer"));
    } else if constexpr (std::is_floating_point<To>::value) {
      return detail::parse_if(from,
        to,
        &c4::csubstr::is_real,
        c4::to_csubstr(" is not a real number"));
    } else {
      static_assert(detail::dependent_false<To>::value,
        "unsupported fundamental type");
    }
  }

  template <typename To>
  static result<void> deserialize(const ryml::ConstNodeRef &from, To *to) {
    using type = omni::compat::remove_cvref_t<To>;
    static_assert(!std::is_pointer<type>::value,
      "destination must not be a pointer");

    if constexpr (std::is_fundamental<type>::value) {
      return deserialize_fundamental(from, to);
    } else if constexpr (detail::is_specialization<std::basic_string,
                           type>::value) {
      return detail::deserialize_string(from, to);
    } else if constexpr (detail::is_specialization<std::vector, type>::value) {
      return detail::deserialize_sequence<algorithm>(from, to);
    } else if constexpr (detail::is_specialization<detail::object,
                           type>::value) {
      return detail::deserialize_object<algorithm>(from, to);
    } else if constexpr (omni::is_reflected<type>::value) {
      return detail::deserialize_reflected<algorithm>(from, to);
    } else {
      static_assert(detail::dependent_false<type>::value,
        "unsupported deserialization type");
    }
  }
};

constexpr detail::deserialize_t<algorithm> deserialize{};

} // namespace if_constexpr
} // namespace serialization
