# Omnirefl

![Omnirefl](omnirefl-banner.png)

A C++ reflection tool built for a seamless experience without macros or UB.

## Sneak Peek

Minimal CMake setup:

```cmake
# 3.18.2 is the current project floor for CMake APIs used by the package and
# reflected target integration.
cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(comprehensive_guide LANGUAGES CXX)

find_package(omnirefl CONFIG REQUIRED)

add_executable(comprehensive_guide comprehensive_guide.cpp)
set_property(TARGET comprehensive_guide PROPERTY CXX_STANDARD 20)
target_link_libraries(comprehensive_guide PRIVATE GTest::GTest gtest_main)

# Reflection is not transitive: only this target's own .cpp files are
# instrumented. Call omni_reflected_target for each target that should be
# reflected.
omni_reflected_target(comprehensive_guide)
```

See [tests/tool/comprehensive_guide/CMakeLists.txt](tests/tool/comprehensive_guide/CMakeLists.txt)
for the executable guide CMake setup.

An equivalent direct tool invocation is:

```bash
# Generate the reflection header, then force-include it when compiling the same
# translation unit.
flags="-std=c++20 -I/path/to/omnirefl/include -I/path/to/gtest/include"
omnirefl -o main.omnirefl.hpp --source main.cpp -- c++ $flags -c main.cpp -o main.o
c++ $flags -include main.omnirefl.hpp main.cpp -lgtest -lgtest_main -pthread -o example && ./example
```

```cpp
#include <gtest/gtest.h>

#include <omnirefl/reflection.hpp>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace example {

enum class status {
  draft,
  active,
};

struct foobar_record {
  int foo_count;
  int bar_count;
  int untouched_count;
};

} // namespace example

TEST(example, foobar) {
  const auto write_foobar = [](omni::binding auto b)
    // The trailing return type keeps the visitor body from being instantiated
    // during the tool run, before generated reflection metadata exists.
    -> example::foobar_record {
      // Reflected scope: this body is instantiated after `reflected_call`
      // instruments the argument type and omnirefl generates its metadata.
      omni::compat::apply(
        [](omni::field_binding auto... field) {
          const auto write = [](omni::field_binding auto field) {
            if constexpr (std::string_view{field.name()}.find("foo")
              != std::string_view::npos)
              field.set_value(8);

            if constexpr (std::string_view{field.name()}.find("bar")
              != std::string_view::npos)
              field.set_value(15);
          };

          (write(field), ...);
        },
        b.public_fields());

      return std::move(b.value);
    };

  const auto record = omni::reflected_call(write_foobar,
    example::foobar_record{
      .foo_count = 1,
      .bar_count = 2,
      .untouched_count = 3,
    });

  EXPECT_EQ(8, record.foo_count);
  EXPECT_EQ(15, record.bar_count);
  EXPECT_EQ(3, record.untouched_count);
}

TEST(example, enum_names) {
  using namespace std::string_view_literals;

  EXPECT_EQ("draft"sv,
    omni::reflected_call(
      [](const omni::binding auto status) -> std::string_view {
        const auto enumerators = status.enumerators();
        const auto it = std::ranges::find(enumerators,
          status.value,
          [](const auto &value_name) { return value_name.first; });

        return enumerators.end() == it ? "unknown"sv : it->second;
      },
      example::status::draft));

  const std::vector<std::pair<example::status, std::string_view>>
    k_expected_pairs{
      {example::status::draft, "draft"sv},
      {example::status::active, "active"sv},
    };

  EXPECT_EQ(k_expected_pairs,
    omni::reflected_call(
      [](omni::meta auto status)
        -> std::vector<std::pair<example::status, std::string_view>> {
        const auto enumerators = status.enumerators();
        return {enumerators.begin(), enumerators.end()};
      },
      omni::type<example::status>));
}
```

See [tests/tool/comprehensive_guide/comprehensive_guide.cpp](tests/tool/comprehensive_guide/comprehensive_guide.cpp)
for the executable guide.

## Seamless Experience

- CMake integration via `omni_reflected_target(...)`.
- Generated reflection headers are force-included for the reflected translation
  unit.
- Dependency files are emitted for generated headers, so build tools rerun
  omnirefl when the source file or included reflection-relevant headers change
  (tested with Ninja only as of this writing).
- No user macros and no reliance on compiler-specific UB for the supported
  release path.

## Supported Scope

Omnirefl focuses on POD-like records and enums for serialization-style
workflows.

- C++11 through C++23, with the most ergonomic visitor syntax in C++20.
- Globally accessible named records and enums.
- Nested named records/enums of supported globally accessible parents.
- Primary record templates.
- Public data fields: names, type names, annotations, read access, and write
  access for mutable fields.
- Enumerator names, values, and annotations.
- Dependency discovery through public field types, public bases, transitive
  public bases, template-record fields, CRTP bases, supported member aliases
  (`error_type`, `key_type`, `mapped_type`, `type`, `value`, `value_type`),
  and template-pack routes named `tuple` or `variant`. Standard-library record
  types are not traversed as reflectable records outside those protocol routes.

### Limits

- `reflected_call` is the instrumentation boundary. Visitor implementations
  must be templates, and lambdas must use trailing return types so the body is
  not instantiated during the tool run before generated metadata exists.
  `constexpr auto result = reflected_call(...)` is not supported: it forces
  evaluation and breaks that instrumentation boundary.
- Direct recursive `reflected_call` is not supported inside a reflected scope.
  A nested reflection call can only work if that reflected path was already
  instantiated independently; do not rely on recursion as an instrumentation
  mechanism.
- Reflection queries are valid only inside the reflected scope. The tool reports
  out-of-scope queries as errors on a best-effort basis.
- Public data members only. Private/protected fields are skipped, including
  fields inherited through public bases. Member functions are not reflected.
- Local and unnamed types are not supported. Experimental indexed support exists
  for investigation, but is not part of the release contract.

Linters and language servers such as clangd can report temporary "ghost"
diagnostics between edits/tool runs, because reflected `.cpp` files depend on
the generated header that is force-included during normal compilation.

## Install

TODO: document package installation / unpacking.

## API Overview

TODO: document `reflected_call`, reflected scope visitors, `meta_t`,
`binding_t`, field metadata, field bindings, and C++20 concepts.

## Examples

TODO: link to the executable usage guide once
`tests/tool/comprehensive_guide/comprehensive_guide.cpp` is implemented.

## Tested Toolchains

TODO: summarize CI coverage and link to the workflow.

# Release Scope

## Minimal Release

### Types

- (+) named globally accessible records
- (+) named globally accessible enums
- (+) nested named records/enums of supported globally accessible parents
- (+) primary record templates, including type, value, and template-template
  parameters
- (+) observed concrete instantiations of supported primary record templates

### Records

- (+) public data fields
- (+) read public fields
- (+) write public non-const fields
- (+) inherited public fields
- (+) transitive public base fields
- (+) public CRTP base fields through supported primary templates
- (+) multiple public base fields
- (+) public base fields from template records
- (+) private/protected base fields are not reflected through derived records
- (+) non-public fields are not reflected
- (+) field names
- (+) field types
- (+) canonical field type names
- (+) only canonical field type names are collected; alias spelling such as
  `std::uint16_t` is not preserved
- (+) field count / iteration
- (+) field index is local to the declaring record, not the flattened inherited
  field tuple
- (+) record `type_name()` without namespaces, including enclosing records
- (+) record `qualified_type_name()` with namespaces and enclosing records

### Enums

- (+) enumerator names
- (+) enumerator values
- (+) scoped enums
- (+) fixed-underlying enums
- (+) enum `type_name()` without namespaces
- (+) enum `qualified_type_name()` with namespaces
- (?) plain unscoped enum field dependencies

### Dependency Routes

- (+) public field type dependencies
- (+) public base type dependencies
- (+) transitive public base dependencies
- (+) standard-library public bases are ignored as reflection dependencies
- (+) template record field dependencies
- (+) CRTP base dependencies
- (+) supported member alias dependencies: `error_type`, `key_type`,
  `mapped_type`, `type`, `value`, `value_type`
- (+) supported template-pack dependencies for template names exactly `tuple`
  and `variant`
- (+) `std::` record types are ignored outside the supported protocol routes
- (+) recursive dependency walk through supported routes

### Serialization Completeness

- (+) reflect record field names
- (+) reflect record field types
- (+) reflect canonical record field type names
- (+) reflect record and field annotations
- (+) reflect documentation comment annotations from `///`, `//!`,
  `/** */`, `/*! */`, `///<`, and `//!<`
- (+) read field values
- (+) write field values
- (+) write inherited public field values
- (+) reflect enum names
- (+) reflect enum values
- (+) reflect enum annotations
- (+) recurse into reflected record fields
- (+) recurse through supported dependency routes

### Frontend/API

- (+) `reflected_call` as the supported reflection instrumentation interface
- (+) `meta_t`, `binding_t`, `field_meta_t`, and `field_binding_t` public
  reflection interfaces
- (+) C++20 `meta`, `binding`, `field_meta`, and `field_binding` concepts

### Build/Release

- TODO(High): add a dedicated usage/comprehensive guide `.cpp` as gtest tests,
  and reference it from this README for in-depth usage demonstrations.
- (+) generated-header reflection
- (+) CMake integration
- (+) annotations enabled by default
- (+) annotations can be disabled with `--no-annotations` or CMake
  `NO_ANNOTATIONS`
- (+) Linux package/install matrix
- (+) Windows package/install test
- (+) GCC and Clang package/install tests

## Planned After Minimal Release

### Types

- (-) nested records inside record template parents
- (-) explicit record template specializations
- (-) partial record template specializations
- (-) specialization-specific record template metadata
- (-) specialization-specific CRTP base metadata

### Metadata

- (-) OpenAPI-like schema table generation from reflected structs/enums using
  type, field, enum, and annotation metadata
- (-) specialization-aware reflected type names when/if explicit or partial
  specializations are implemented

### Frontend/API

- TODO(High): forbid `reflected_call` inside a reflected scope. With the current
  deferred visitor implementation, reliable detection is not practically
  possible without instantiating visitor bodies or adding a broader semantic
  analysis pass.
- TODO(High): detect reflection query instantiation outside a reflected scope as
  a tool error when possible, and add dedicated negative tool-run tests.
- (-) refine the public interface
- (-) replace `reflected_call`
- (-) document why reflected-scope visitors sometimes need explicit trailing
  return types to avoid premature instantiation during the tool run
- TODO(High): expose field mutability metadata, at least as a constexpr
  `is_mutable` on field metadata/bindings, so users can filter writable fields
  before calling `set_value`.
- (-) separate const and mutable public-field accessors in the public interface
- (-) recoverable reflection query/fallback branch for non-reflected types
- (-) type-erased field wrappers, likely short `field_t`-style names
- (-) refine the CLI interface
- (-) remove dependency on compilation database
- (-) make the tool callable like a compiler instance with limited support for
  compilation-meaningful flags
- (-) Unix-like invocation: `omnirefl -o <reflection.hpp> -MF <deps.d> -- <cc1 args...>`
- (-) split compiler-driver/compile-db args to cc1 mapping into a separate
  composable tool

### Supported Toolchains

Current state is reported by the
[CI workflow](https://github.com/sergio-eld/omnirefl/actions/workflows/ci.yml).

- (+) `Linux:Alpine GCC` covered by CI package matrix
- (+) `Linux:Alpine Clang` covered by CI package matrix
- (+) `Linux:Alpine MinGW GCC` covered by CI package matrix
  (build-only for Windows test binaries)
- (+) `Linux:Ubuntu 18.04 GCC` covered by CI package matrix
- (+) `Linux:Ubuntu 18.04 Clang` covered by CI package matrix
- (+) `Linux:Ubuntu 20.04 GCC` covered by CI package matrix
- (+) `Linux:Ubuntu 20.04 Clang` covered by CI package matrix
- (+) `Linux:Ubuntu 22.04 GCC` covered by CI package matrix
- (+) `Linux:Ubuntu 22.04 Clang` covered by CI package matrix
- (+) `Linux:Ubuntu MinGW GCC` covered by CI package matrix
  (build-only for Windows test binaries)
- (+) `Windows:MSVC` covered by CI package matrix
- (+) `Windows:clang-cl` covered by CI package matrix
- (+) `Windows:MSYS2 MinGW` covered by CI package matrix
- (+) `Windows:MSYS2 clang` covered by CI package matrix
- TODO(High): cross-build the tool for `macOS`.
- TODO(High): cross-build the tool for `ARM64` targets.
- TODO(High): investigate switching the tool build to Cosmopolitan after a
  baseline benchmark is in place.

### Continuous Benchmark

Benchmark runs are reported by the
[CI workflow](https://github.com/sergio-eld/omnirefl/actions/workflows/ci.yml).

- Target: `linux-x86_64`
- Environment: Ubuntu 22.04 GCC package-test image
- Baseline target: `benchmark.baseline`
- Raw history artifact: `benchmark-history-linux-x86_64`
- Reported baseline: average of the last 5 stored runs
- Tracked metrics:
  - reflection/tool wall time for `benchmark.baseline.omni`
  - build wall time for `benchmark.baseline`
  - reflection/tool wall time as percentage of build wall time
- TODO: when the repository goes public, render or link the benchmark history
  directly from the README instead of requiring artifact lookup.

## Might Be Considered Later

- (-) unnamed non-local types addressable from namespace scope via
  `decltype(symbol)`
- (-) unnamed non-local types addressable through function return type
- (-) other globally addressable unnamed cases

## Out

- (-) private/protected fields
- (-) methods
- (-) local/block-scope types
- (-) arbitrary composed `reflected_call` instrumentation
- (-) recursive reflection
