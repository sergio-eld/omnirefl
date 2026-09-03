#pragma once

#include "common.hpp"

#include <string>
#include <type_traits>
#include <vector>

namespace serialization {
namespace if_constexpr {

template <typename To>
compat::expected<To, std::string> parse_fundamental(
  const ryml::ConstNodeRef &from) {
  const c4::csubstr value = from.val();
  const auto format_error = [value](const char *what) {
    return compat::unexpected(
      '"' + detail::to_string(value) + "\" " + what);
  };

  if constexpr (std::is_same<To, bool>::value) {
    if (!detail::is_boolean(from, value))
      return format_error("is not a boolean");
  } else if constexpr (std::is_integral<To>::value
    && std::is_unsigned<To>::value) {
    if (!value.is_unsigned_integer())
      return format_error("is not an unsigned integer");
  } else if constexpr (std::is_integral<To>::value) {
    if (!value.is_integer())
      return format_error("is not an integer");
  } else if constexpr (std::is_floating_point<To>::value) {
    if (!value.is_real())
      return format_error("is not a real number");
  } else {
    static_assert(detail::dependent_false<To>::value,
      "unsupported fundamental type");
  }

  compat::expected<To, std::string> result{};
  if (!c4::from_chars(value, &result.value()))
    return format_error("could not be converted to the destination type");

  return result;
}

struct deserialize_t {
  template <typename To>
  static compat::expected<void, std::string> deserialize(
    const ryml::ConstNodeRef &from, To *to) {
    using type = omni::compat::remove_cvref_t<To>;
    static_assert(!std::is_pointer<type>::value,
      "destination must not be a pointer");

    if constexpr (std::is_fundamental<type>::value) {
      const auto parsed = if_constexpr::parse_fundamental<type>(from);
      if (!parsed)
        return compat::unexpected(parsed.error());

      *to = parsed.value();
      return {};
    } else if constexpr (detail::is_specialization<std::basic_string,
                           type>::value) {
      return detail::deserialize_string(from, to);
    } else if constexpr (detail::is_specialization<std::vector, type>::value) {
      return detail::deserialize_sequence<deserialize_t>(from, to);
    } else if constexpr (detail::is_specialization<detail::object,
                           type>::value) {
      return detail::deserialize_object<deserialize_t>(from, to);
    } else if constexpr (omni::is_reflected<type>::value) {
      return detail::deserialize_reflected<deserialize_t>(from, to);
    } else {
      static_assert(detail::dependent_false<type>::value,
        "unsupported deserialization type");
    }
  }

  template <typename To>
  compat::expected<void, std::string> operator()(
    const ryml::ConstNodeRef &from, To *to) const {
    return omni::reflected_call(
      detail::reflected_record<deserialize_t>{from}, *to);
  }

  template <typename To>
  compat::expected<To, std::string> to(const ryml::ConstNodeRef &from) const {
    To value{};
    const auto deserialized = (*this)(from, std::addressof(value));
    if (!deserialized)
      return compat::unexpected(deserialized.error());

    return compat::expected<To, std::string>{std::move(value)};
  }
};

constexpr deserialize_t deserialize{};

} // namespace if_constexpr
} // namespace serialization
