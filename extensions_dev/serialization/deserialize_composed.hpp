#pragma once

#include "common.hpp"

#include <omnirefl/functional.hpp>

#include <string>
#include <type_traits>
#include <vector>

namespace serialization {
namespace composed {

template <typename To>
compat::expected<To, std::string> parse_fundamental(ryml::ConstNodeRef from) {
  namespace fn = omni::fn;
  using node_or_error = compat::expected<ryml::ConstNodeRef, std::string>;
  static_assert(std::is_integral<To>::value
      || std::is_floating_point<To>::value,
    "unsupported fundamental type");

  // TODO: Add negative-path benchmarks first so diagnostics changes are
  // measurable. Replace eager error strings with structured diagnostics; do
  // not stringify `from.val()` blindly because a non-scalar node may make that
  // unexpectedly expensive. Assemble messages from the node-owned source
  // buffer and static diagnostic literals, and report the complete field path,
  // including leaf names and array indices, before release. Consider a policy
  // that accumulates errors but stops after a configurable limit.
  const auto error = [from](const char *what) -> node_or_error {
    return compat::unexpected(
      '"' + serialization::detail::to_string(from.val()) + "\" " + what);
  };

  return fn::cond( //
    fn::when(std::is_same<To, bool>{},
      [from, error]() -> node_or_error {
        return serialization::detail::is_boolean(from, from.val())
          ? node_or_error{from}
          : error("is not a boolean");
      }),
    fn::when(std::is_unsigned<To>{},
      [from, error]() -> node_or_error {
        return from.val().is_unsigned_integer()
          ? node_or_error{from}
          : error("is not an unsigned integer");
      }),
    fn::when(std::is_integral<To>{},
      [from, error]() -> node_or_error {
        return from.val().is_integer() //
          ? node_or_error{from}
          : error("is not an integer");
      }),
    fn::when(std::is_floating_point<To>{},
      [from, error]() -> node_or_error {
        return from.val().is_real() //
          ? node_or_error{from}
          : error("is not a real number");
      }),
    [from]() -> node_or_error { return node_or_error{from}; })
    .and_then( //
      [error](ryml::ConstNodeRef node) -> compat::expected<To, std::string> {
        compat::expected<To, std::string> value{};
        if (!c4::from_chars(node.val(), &value.value()))
          return compat::unexpected(
            error("could not be converted to the destination type").error());

        return value;
      });
}

namespace detail {

template <typename Condition, typename Type>
struct _case {};

template <typename... Case>
struct select;

template <typename Fallback>
struct select<Fallback> {
  using type = Fallback;
};

template <typename Condition, typename Type, typename... Case>
struct select<_case<Condition, Type>, Case...> {
  using type = omni::compat::conditional_t<Condition::value,
    Type,
    typename select<Case...>::type>;
};

template <typename... Case>
using select_t = typename select<Case...>::type;

// C++11 type dispatch must defer these bodies until the selected branch is
// invoked; captured lambdas inside `deserialize<To>` instantiate immediately.
template <typename To>
struct deserialize_fundamental {
  ryml::ConstNodeRef from;
  To *to;

  compat::expected<void, std::string> operator()() const {
    const auto parsed = parse_fundamental<To>(from);
    if (!parsed)
      return compat::unexpected(parsed.error());

    *to = parsed.value();
    return {};
  }
};

template <typename To>
struct deserialize_string {
  ryml::ConstNodeRef from;
  To *to;

  compat::expected<void, std::string> operator()() const {
    return serialization::detail::deserialize_string(from, to);
  }
};

template <typename Deserialize, typename To>
struct deserialize_sequence {
  ryml::ConstNodeRef from;
  To *to;

  compat::expected<void, std::string> operator()() const {
    return serialization::detail::deserialize_sequence<Deserialize>(from, to);
  }
};

template <typename Deserialize, typename To>
struct deserialize_object {
  ryml::ConstNodeRef from;
  To *to;

  compat::expected<void, std::string> operator()() const {
    return serialization::detail::deserialize_object<Deserialize>(from, to);
  }
};

template <typename Deserialize, typename To>
struct deserialize_reflected {
  ryml::ConstNodeRef from;
  To *to;

  compat::expected<void, std::string> operator()() const {
    return serialization::detail::deserialize_reflected<Deserialize>(from, to);
  }
};

template <typename To>
struct unsupported {
  unsupported(ryml::ConstNodeRef, To *) {}

  compat::expected<void, std::string> operator()() const {
    static_assert(serialization::detail::dependent_false<To>::value,
      "unsupported deserialization type");
    return {};
  }
};

} // namespace detail

struct deserialize_t {
  template <typename To>
  static compat::expected<void, std::string>
    deserialize(const ryml::ConstNodeRef &from, To *to) {
    using type = omni::compat::remove_cvref_t<To>;
    static_assert(!std::is_pointer<type>::value,
      "destination must not be a pointer");

    using operation = detail::select_t< //
      detail::_case< //
        std::is_fundamental<type>,
        detail::deserialize_fundamental<type>>,
      detail::_case< //
        serialization::detail::is_specialization<std::basic_string, type>,
        detail::deserialize_string<type>>,
      detail::_case< //
        serialization::detail::is_specialization<std::vector, type>,
        detail::deserialize_sequence<deserialize_t, type>>,
      detail::_case< //
        serialization::detail::is_specialization<serialization::detail::object,
          type>,
        detail::deserialize_object<deserialize_t, type>>,
      detail::_case< //
        omni::is_reflected<type>,
        detail::deserialize_reflected<deserialize_t, type>>,
      detail::unsupported<type>>;

    return operation{from, to}();
  }

  template <typename To>
  compat::expected<void, std::string> operator()(const ryml::ConstNodeRef &from,
    To *to) const {
    return omni::reflected_call(
      serialization::detail::reflected_record<deserialize_t>{from},
      *to);
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

} // namespace composed
} // namespace serialization
