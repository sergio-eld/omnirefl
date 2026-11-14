# Codebase conventions

This document describes naming, API design, formatting, documentation, and
testing conventions used in Omnirefl.

## Contents

- [Types, objects, traits, and concepts](#types-objects-traits-and-concepts)
- [`tdm_` prefix: functional data-member updates][tdm-updates]
- [Constraints](#constraints)
- [Customization points](#customization-points)
  - [Dispatchers](#dispatchers)

[tdm-updates]: #tdm_-prefix-functional-data-member-updates

## Types, objects, traits, and concepts

- `<symbol>_t`: a template type, or a type with a global default object named
  `<symbol>`.
- `<symbol>`: the default object or variable template.
- `is_<symbol><T>`: a Boolean type trait.
- `is_<symbol>_v<T>`: its Boolean value.
- `<symbol>_like<T>`: a concept.

```cpp
#include <type_traits>

template <typename T>
struct config_t {};

template <typename T>
struct is_config : std::false_type {};

template <typename T>
struct is_config<config_t<T>> : std::true_type {};

template <typename T>
inline constexpr bool is_config_v = is_config<T>::value;

template <typename T>
concept config_like = is_config_v<T>;

struct operation_t {};
constexpr operation_t operation{};
```

## `tdm_` prefix: functional data-member updates

QoL for functional inplace modifications: update a data member within a single
expression, returning a new object. `const &` overloads preserve the source;
`&&` overloads may move from it.

- `tdm_<member>(f)` immediately computes `f(current_member)` and returns a
  configured object.
- Constrain updater invocation against the current member.

Provide `const &` and `&&` overloads when data members may be movable: the
`const &` overload copies unchanged members, while the `&&` overload moves them
from the source.

```cpp
#include <concepts>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

template <typename Host>
struct config_t {
  Host host;
  unsigned port;

  template <std::invocable<const Host &> F>
  constexpr auto tdm_host(F &&f) const & {
    return ::config_t{
      .host = std::invoke(std::forward<F>(f), this->host),
      .port = this->port,
    };
  }

  template <std::invocable<Host &&> F>
  constexpr auto tdm_host(F &&f) && {
    return ::config_t{
      .host = std::invoke(std::forward<F>(f), std::move(this->host)),
      .port = this->port,
    };
  }

  template <std::invocable<const unsigned &> F>
  constexpr auto tdm_port(F &&f) const & {
    return ::config_t{
      .host = this->host,
      .port = std::invoke(std::forward<F>(f), this->port),
    };
  }

  template <std::invocable<unsigned &> F>
  constexpr auto tdm_port(F &&f) && {
    return ::config_t{
      .host = std::move(this->host),
      .port = std::invoke(std::forward<F>(f), this->port),
    };
  }
};

namespace global {

using namespace std::string_view_literals;

// C++20: designated initialization with CTAD deduces Host = std::string_view.
constexpr config_t config{.host = "api"sv, .port = 8080};

}

const auto configured = global::config
  .tdm_host([](auto old) { return std::string{old} + ".example.com"; })
  .tdm_port([](auto old) { return old + 1; }); //< `.host` is moved.
// configured.host is std::string{"api.example.com"}.
// configured.port is 8081.
```

The example uses C++20 for brevity; implementations should also provide
C++11-compatible versions.

## Constraints

If a function should be constrained, typically as part of a public API, and
`requires` is beneficial over `enable_if`, `OMNI_CONSTRAINED_TEMPLATE` may be
used to avoid compatibility-guard boilerplate. It expands to `requires` in
C++20 and SFINAE in earlier standards.

- Keep constraints from obscuring return types.
- Prefer readable diagnostics near the public call site.

```cpp
#include <omnirefl/compat.hpp>
#include <type_traits>

OMNI_CONSTRAINED_TEMPLATE((typename T), (std::is_arithmetic<T>::value))
constexpr T twice(T value) {
  return value + value;
}
```

<!-- REFACTORME: [high] Revisit the customization-point conventions and example.
-->

## Customization points

The `cp` (sub-)namespace is designated for user customization.

Global SFINAE-based selection should follow these registration forms:

- C++14 and later (preferred): specialize a `const` variable template, using
  `constexpr` when possible. This is easier to customize than a type: users can
  provide a callable expression directly, without defining a class and its
  `operator()`.
- C++11: specialize a class template.

For a given type, specialize only one global customization point: the variable
template or the class template. The examples below select different types.

```cpp
#include <concepts>
#include <type_traits>

namespace core {
namespace cp {

struct unavailable {};

// C++11 customization point.
template <typename T, typename = void>
struct twice_t : unavailable {};

// C++14 and later customization point.
template <typename T, typename = void>
constexpr unavailable twice{};

} // namespace cp

template <typename T>
struct is_unsigned_integer : std::bool_constant<
  std::is_integral_v<T> && std::is_unsigned_v<T>
  && !std::is_same_v<T, bool>> {};

template <typename T>
  requires is_unsigned_integer<T>::value
constexpr T default_twice(T value) {
  // Unsigned overflow wraps; this behavior is acceptable for the example.
  return static_cast<T>(value + value);
}

// Example dispatcher.
template <typename T>
constexpr auto twice(T value) {
  constexpr bool has_type =
    !std::is_base_of_v<cp::unavailable, cp::twice_t<T>>;
  constexpr bool has_variable = !std::is_same_v<cp::unavailable,
    std::remove_cv_t<decltype(cp::twice<T>)>>;

  static_assert(has_type || has_variable || is_unsigned_integer<T>::value,
    "No default: specialize core::cp::twice<T> or core::cp::twice_t<T>");
  static_assert(!(has_type && has_variable),
    "Customize either cp::twice_t<T> or cp::twice<T>, not both");

  if constexpr (has_variable && !has_type)
    return cp::twice<T>(value);
  else if constexpr (has_type && !has_variable)
    return cp::twice_t<T>{}(value);
  else if constexpr (is_unsigned_integer<T>::value)
    return default_twice(value);
}

} // namespace core

// C++11: signed integers use a struct with operator() boilerplate.
template <typename T>
struct core::cp::twice_t<T,
  typename std::enable_if<std::is_integral<T>::value
    && std::is_signed<T>::value>::type> {
  constexpr T operator()(T value) const { return value + value; }
};

// C++20 for brevity; C++14/17 use enable_if instead of requires.
// A lambda is sufficient.
template <std::floating_point T>
constexpr auto core::cp::twice<T> = [](std::floating_point auto v) {
  return v + v;
};

struct my_uint {
  unsigned value;
};

template <>
constexpr auto core::cp::twice<my_uint> = [](my_uint m) {
  // Reuse the default directly, bypassing customization lookup.
  return my_uint{core::default_twice(m.value)};
  // Also possible, but re-enters customization lookup for unsigned:
  // Use with caution: the selected customization must not delegate back here.
  // return core::twice(m.value);
  // This returns unsigned; wrap it in my_uint to preserve the wrapper type.
};

const auto integer = core::twice(21); //< Class specialization: 42.
const auto real = core::twice(2.5); //< Variable specialization: 5.0.
const auto natural = core::twice(21u); //< Default implementation: 42u.
const auto wrapped = core::twice(my_uint{21u}); //< my_uint{42u}.
// const auto unsupported = core::twice(true); //< No customization or default.
```

The example uses C++20 for brevity. The core implementation must remain
C++11-compatible; user customizations may use any features available in the
standards they support.

Users may register concrete types or template families. Shared operation
customization points reside in `traits::cp`.

- Keep customization primaries unavailable. If no user customization is
  registered, the dispatcher should attempt a separate default implementation
  where one exists. Keep detection inside default implementations so user
  specializations do not compete with built-in defaults.
- Detection traits such as `traits::is_optional<T>` check the required adapted
  operations and associated types. User types need not inherit a base class.
- Document accepted arguments, normalized result types, and ownership or
  lifetime requirements for each operation.

TODO: Explore a defaulted local-override accessor argument to `core::twice`.
An explicitly supplied accessor could override global or default selection,
or supply a customization when neither applies. The design is still in
progress; this is the intended pattern for local overrides in `map_tree`
configuration.

### Dispatchers

- Expose operations through dispatchers such as `core::twice(value)`;
  callers need not construct customization objects explicitly.
- For a closed set of implementation alternatives, prefer a `_case` helper
  with template specializations over same-name function overloads. Keep one
  entry point that selects the appropriate case at compile time.

```cpp
#include <cstddef>
#include <type_traits>

namespace detail {

template <bool IsArray>
struct size_case;

template <>
struct size_case<true> {
  template <typename T, std::size_t N>
  constexpr auto operator()(const T (&)[N]) const { return N; }
};

template <>
struct size_case<false> {
  constexpr auto operator()(const auto &value) const { return value.size(); }
};

} // namespace detail

constexpr auto size(const auto &value) {
  using T = std::remove_cvref_t<decltype(value)>;
  return detail::size_case<std::is_array_v<T>>{}(value);
}
```
