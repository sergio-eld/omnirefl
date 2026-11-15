# Codebase conventions

This document describes naming, API design, formatting, documentation, and
testing conventions used in Omnirefl.

## Contents

- [Code style](#code-style)
- [Types, objects, traits, and concepts](#types-objects-traits-and-concepts)
- [`tdm_` prefix: functional data-member updates][tdm-updates]
- [Constraints](#constraints)
- [Customization points](#customization-points)
  - [Dispatchers](#dispatchers)
- [Guides and examples](#guides-and-examples)
  - [Guide structure](#guide-structure)
  - [Standalone projects](#standalone-projects)
- [Extension structure](#extension-structure)
- [Standard compatibility](#standard-compatibility)
- [Value-based metaprogramming](#value-based-metaprogramming)

[tdm-updates]: #tdm_-prefix-functional-data-member-updates

## Code style

Names use `snake_case`, following the standard library: namespaces, types,
concepts, functions, data members, variables, and enumerators. `CamelCase` is
reserved for template parameters, such as `Value` or `BufferSize`.

Macros use `OMNI_UPPER_SNAKE_CASE`.

Single-letter parameters and locals may be used when their roles are clear
and nothing in the scope is reassigned, regardless of scope length. Descriptive
names help distinguish values of the same or convertible types and follow
changing state. Mutating member calls alone do not require longer names.

Two single-letter names may represent distinct source and destination roles:

```cpp
#include <iostream>

struct source { int value; };
struct destination { int doubled; };

destination convert(const source &a) {
  const destination b{.doubled = a.value * 2};
  // Just an exposition to justify creating b instead of returning directly.
  std::cout << "Converted " << a.value << " to " << b.doubled << '\n';
  return b;
}
```

Conventional short names include `i`, `j`, and `k` for indices, `n` for a count,
`p` for a pointer, and `e` for an error.

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

For separate declarations and definitions, the first declaration uses
`OMNI_CONSTRAINED_TEMPLATE`; subsequent declarations and the definition use
`OMNI_CONSTRAINED_TEMPLATE_REDECL` with identical parameters and constraints:

```cpp
OMNI_CONSTRAINED_TEMPLATE((typename T), (std::is_arithmetic<T>::value))
T twice(T value);

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T), (std::is_arithmetic<T>::value))
T twice(T value) { return value + value; }
```

This is an ad hoc compatibility distinction: before C++20, the defaulted
SFINAE parameter can receive its default only once. C++20 instead repeats the
requires-clause. With C++20 as the baseline, direct requires-clauses replace
these adapters.

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
progress; this is the intended pattern for local overrides in `map_by_schema`
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

## Guides and examples

Prefer **guide** for "how to" recipes teaching the interface, and **example**
for a concrete application or use case. This is a naming guideline, not a strict
division; a project may serve both purposes.

### Guide structure

A guide presents the public interface through a TEST-oriented, cohesive
top-down narration, starting with the basic ideas and building toward more
involved uses. Comments explain the ideas, examples show them in code, and
GTest assertions make the expected results explicit while keeping the guide
executable.

The opening block states the scope and language baseline (e.g. C++11), then lists
`suite.topic - description` entries in source order, grouped by section.
Searching for `TEST(suite, topic)` or `TEST_P(suite, topic)` finds the entry
point even when namespaces share its name. Ordinary tests run with
`--gtest_filter=suite.topic`; parameterized instances use their generated names
or a wildcard filter such as `--gtest_filter='*suite.topic*'`.

The core guide introduces reflection concepts before building toward
inspection, access, and modification.

A shortened example of an opening block for the core reflection guide:

```cpp
/** Executable reflection guide. Examples use C++20 syntax; the interfaces
 * remain C++11-compatible.
 * Search for TEST(suite, topic) for each entry below.
 *
 * Basics
 *   basics.reflected_scope - introduce reflected_call with a generic visitor
 * Advanced
 *   advanced.binding_ownership - inspect ownership and constrain callables
 */
```

Extensions provide higher-level utilities intended to work out of the box:

- **Basics:** roughly 90% of immediately useful usage, fewest extra steps first;
  default interface before configuration and common customization.
- **Advanced:** deeper customization and other topics by practical relevance,
  after their prerequisites.
- **Application examples:** separate from the Basics/Advanced progression,
  or in separate example files.

**Compatibility in guides and examples:**

- Newer syntax may be used for brevity, with the supported baseline stated.
- Earlier-standard equivalents appear once near the beginning.
- New version-specific idioms have local alternatives, possibly commented out:

```cpp
const auto result = omni::ryml::deserialize(omni::type<int>, "42");
// Before C++14:
// const auto result = omni::ryml::deserialize(omni::type_t<int>{}, "42");
```

A topic's introductory annotation sits above its `TEST` entry point. Essential
model declarations and expected results stay nearby; operation-specific
explanations sit beside the relevant expression. The contents stay synchronized
with topic names, descriptions, and source order as the guide changes.

### Standalone projects

Each project under `extras/examples/` must be self-contained: users can copy its
directory and configure it independently. Apply the same requirement to
standalone extension guides and examples.

- Include the project's own `CMakeLists.txt`, sources, headers, and data.
- Declare dependencies through package discovery or fetching. Use Omnirefl's
  installed public CMake interface, without depending on repository-internal
  scripts or the parent project's configuration.
- Do not generate a standard matrix inside a guide or example project.
- Require any specific language version in the project's CMake configuration:

```cmake
set_target_properties(example PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES)
```

When no particular version is required, invoke the shared
lowest-default-standard CMake helper. This permitted shared policy helper must
be available to copied standalone projects.

The parent `CMakeLists.txt` checks whether a guide/example can be included
before calling `add_subdirectory`. Report exclusions with
`message(STATUS ...)`, stating the project and the unmet requirement.
Standalone projects still declare their
own requirements so they do not rely on those parent checks.

## Extension structure

Extension headers under `extensions/<module>/include/omnirefl/` ship in the
Omnirefl packages, installed under `<includedir>/omnirefl/`. Each extension has
a CMake component at `<libdir>/cmake/omnirefl/components/<module>.cmake`, alongside
its dependency helpers and patches, and an entry in `Config.cmake.in`.

Components are opt-in. For example:

```cmake
find_package(omnirefl CONFIG REQUIRED COMPONENTS serialization)
```

This resolves the serialization dependencies and provides the
`omni::serialization` interface target.

Guides, tests, benchmarks, and shared example data live under
`extras/extensions/<module>/`. They ship as source in the separate
`omnirefl-extras-<version>-<commit>.tar.gz` archive and build against the
installed package. Keep their required sources and shared headers within that
archive rather than depending on an unshipped repository checkout.

Standalone guides and examples follow the [self-containment rules][guides].

[guides]: #guides-and-examples

Use `<module>/guide.cpp` for a single-file guide. Use
`<module>/guide/main.cpp` when additional files, such as shared headers or
standalone build configuration, warrant a directory.

CMake executable targets follow `<module>.guide`, `<module>.test`, or
`<module>.example`; benchmarks use `<module>.benchmark`. For example, the
RapidYAML-backed JSON guide is `json_ryml.guide`.

Several executables use `<module>.<kind>.<specific_name>`, such as
`json_ryml.guide.aliases`. A main guide may retain `json_ryml.guide` alongside
those specific guides. Existing target prefixes precede this name: standard
matrix variants use `cpp_<standard>.`, for example
`cpp_11.json_ryml.test` and `cpp_20.json_ryml.test`. Standard variants still
represent one logical executable.

Apply the same layout rule to tests and benchmarks. Within each directory:

- Use `main.cpp` when there is only one logical executable. Additional sources
  may contribute to that executable.
- For several independent executables, use descriptive entry-source names such
  as `specific_scenario.cpp`, without a `main.cpp`.
- Different standard configurations of the same executable count as one.
- Framework-provided entry points do not change the naming: `main.cpp` may link
  against GTest's or Benchmark's supplied `main()`.

```text
<module>/
  guide/
    main.cpp
    shared.hpp
  tests/
    main.cpp
    specific_scenario.cpp
  benchmarks/
    main.cpp
    shared.hpp
```

Here, `tests/main.cpp` and `tests/specific_scenario.cpp` form one executable.
For independent scenario executables, use:

```text
<module>/tests/
  first_scenario.cpp
  second_scenario.cpp
  shared.hpp
```

## Standard compatibility

Core and extension modules use standard C++ interfaces, with compatibility
fallbacks for their supported language and library versions. Shared `compat`
names select the standard implementation when available and a compatible
fallback otherwise; calling code does not repeat those guards.

```cpp
#include <type_traits>

#if defined(__has_include)
#  if __has_include(<expected>)
#    include <expected>
#  endif
#endif

#if !defined(__cpp_lib_expected) || __cpp_lib_expected < 202211L
#  include <tl/expected.hpp>
#endif

namespace omni {
namespace compat {

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202211L
template <typename T, typename Error>
using expected = std::expected<T, Error>;
#else
template <typename T, typename Error>
using expected = tl::expected<T, Error>;
#endif

#if defined(__cpp_lib_transformation_trait_aliases)
template <bool Condition, typename T = void>
using enable_if_t = std::enable_if_t<Condition, T>;
#else
template <bool Condition, typename T = void>
using enable_if_t = typename std::enable_if<Condition, T>::type;
#endif

} // namespace compat
} // namespace omni
```

Feature-test macros describe library availability more precisely than the
language version alone. Third-party fallbacks belong to the component that
needs them; this example does not make `expected` a core dependency.

Third-party dependencies of optional components are not bundled with the main
tool. They are fetched only when the user requests the corresponding CMake
component.

<!-- Keep this section last; insert future convention sections before it. -->

## Value-based metaprogramming

Prefer `constexpr` functions and ordinary data over encoding computations in
template instantiations, where possible. This expresses algorithms directly,
without type lists or specializations for intermediate results.

Exception: prefer the template implementation if it is substantially faster to
compile. Establish that advantage with measurements on representative inputs
and relevant compilers.

```cpp
#include <type_traits>

// Type-based computation.
template <unsigned A, unsigned B>
struct gcd_t : gcd_t<B, A % B> {};

template <unsigned A>
struct gcd_t<A, 0> : std::integral_constant<unsigned, A> {};

constexpr auto first = gcd_t<42, 30>::value;

// Value-based computation.
constexpr unsigned gcd(unsigned a, unsigned b) {
  return b ? gcd(b, a % b) : a;
}

constexpr auto second = gcd(42, 30);
```

Both compute at compile time. The function needs no specializations and also
accepts runtime inputs. Templates remain appropriate for type selection,
specialization, and generating different code.
