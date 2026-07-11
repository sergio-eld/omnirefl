# Omnirefl

<p align="center">
  <img src="omnirefl-banner.png" alt="Omnirefl" width="640">
</p>

A C++ reflection tool built for a seamless experience without macros or UB.

## Sneak Peek

Minimal CMake setup:

```cmake
# 3.18.2 is the current project floor for CMake APIs used by the package and
# reflected target integration.
cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(example LANGUAGES CXX)

find_package(omnirefl CONFIG REQUIRED)

add_executable(example main.cpp)
set_property(TARGET example PROPERTY CXX_STANDARD 20)

# Reflection is not transitive: only this target's own .cpp files are
# instrumented. Call omni_reflected_target for each target that should be
# reflected.
omni_reflected_target(example)
```

The same target shape is used by
[tests/tool/example/CMakeLists.txt](tests/tool/example/CMakeLists.txt).

An equivalent direct tool invocation is:

```bash
# Generate the reflection header, then force-include it when compiling the same
# translation unit.
flags="-std=c++20 -I/path/to/omnirefl/include"
omnirefl -o example.omnirefl.hpp --source main.cpp -- c++ $flags -c main.cpp -o example.o
c++ $flags -include example.omnirefl.hpp main.cpp -o example && ./example
```

```cpp
#include <omnirefl/reflection.hpp>

#include <iostream>
#include <string>
#include <string_view>
#include <tuple>

struct record {
  int foo;
  std::string bar;
};

int main() {
  using namespace std::string_view_literals;

  record value{
    .foo = 1,
    .bar = "before",
  };

  std::cout << "before: foo=" << value.foo << " bar=" << value.bar << '\n';

  const auto write =
    [](omni::binding auto b)
    // Generic lambdas used as reflected visitors must spell the return type.
    -> void {
      std::apply(
        [](omni::field_binding auto... field) -> void {
          const auto set = [](omni::field_binding auto field) -> void {
            constexpr std::string_view name = field.name();

            if constexpr ("foo"sv == name)
              field.set_value(8);

            if constexpr ("bar"sv == name)
              field.set_value("after");
          };

          (set(field), ...);
        },
        b.public_fields());
    };

  omni::reflected_call(write, value);

  std::cout << "after: foo=" << value.foo << " bar=" << value.bar << '\n';
}
```

The snippet above is available as
[tests/tool/example/main.cpp](tests/tool/example/main.cpp). It is part of
the packaged test/example source tree, but it is not a CTest test.
See [tests/tool/comprehensive_guide/comprehensive_guide.cpp](tests/tool/comprehensive_guide/comprehensive_guide.cpp)
for the full executable guide. It targets limited C++20 support: omnirefl
concepts are used, but `<concepts>` and C++20 ranges algorithms are avoided.

## Seamless Experience

- CMake integration via `omni_reflected_target(...)`.
- Packaged runnable example and guide sources.
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
  (`error_type`, `first_type`, `key_type`, `mapped_type`, `second_type`, `type`,
  `value`, `value_type`), and template-pack routes named `tuple` or `variant`.
  Standard-library record types are not traversed as reflectable records
  outside those protocol routes.

### Limits

- `reflected_call` is the instrumentation boundary. The visitor must be either
  a generic lambda or a type with a templated `operator()`. Its return type must
  not depend on instantiating the visitor body during the tool run; for lambdas,
  this means an explicit trailing return type, including `-> void`.
  `constexpr auto result = reflected_call(...)` is not supported: it forces
  evaluation and breaks that instrumentation boundary.
- `reflected_call` arguments must be reflected record/enum values, for which
  `meta_t<T>` or `binding_t<T>` is generated, or `omni::type<T>`. Pointers, raw
  arrays, fundamental values, standard-library records, forward declarations
  without definitions, partial template specializations, and compound inputs
  such as `std::tuple<T...>` or `std::vector<T>` are unsupported. Detection is
  best effort because arbitrary compound class templates cannot be reliably
  distinguished from ordinary record templates. The caller is responsible for
  sanitizing inputs: include definitions, dereference pointers, wrap arrays,
  and use `std::visit` or `mpark::visit` to pass a variant's active alternative.
  Compound types may still expose reflected dependencies through the supported
  routes listed above. Incomplete dependency records are skipped with an info
  diagnostic instead of failing the tool run. Unsupported complete dependencies
  are skipped with a warning. When the dependency is a public base, its
  inherited fields are omitted from the generated metadata.
- Direct recursive `reflected_call` is not supported inside a reflected scope.
  A nested reflection call can only work if that reflected path was already
  instantiated independently.
- Reflection queries are valid only inside the reflected scope. The tool reports
  out-of-scope queries as errors on a best-effort basis.
- Public data members only. Private/protected fields are skipped, including
  fields inherited through public bases. Member functions are not reflected.
- Local and unnamed types are not supported. Experimental indexed support exists
  for investigation (`omni_reflected_target(... ENABLE_INDEX_MODE)`), but is
  not part of the release contract: it has proven unstable because function
  template specializations are instantiated lazily, and a dependent return type
  can postpone instantiating the function body until the specialization is
  required.

Linters and language servers such as clangd can report temporary "ghost"
diagnostics between edits/tool runs, because reflected `.cpp` files depend on
the generated header that is force-included during normal compilation.

## Install

Install options:

- Latest release:
  [download the packaged archive for your platform](https://github.com/sergio-eld/omnirefl/releases/latest).
  Linux packages are published as `.deb` and `.tar.gz`; Windows packages are
  published as `.zip`.
- Latest CI artifact:
  open the latest successful `CI` run on `master` and use the artifacts
  produced by `Build package / linux-x86_64` or
  `Build package / windows-x86_64`. GitHub Actions artifacts do not provide a
  stable direct "latest artifact" download URL; look for
  `packages-linux-x86_64-musl-<short-sha>` or
  `packages-windows-x86_64-ucrt-<short-sha>`.
- Build locally using the prepared Docker image; see
  [Build and Develop Locally](#build-and-develop-locally).

The examples below assume a standard install prefix such as `/usr/local`. For
an unpacked package, use the unpacked directory as `prefix`.

## Packaged Tests and Examples

Assuming a standard install, the packaged test/example sources are available
under `share/omnirefl/tests`. Copy them into a writable directory before
configuring:

```bash
prefix=/usr/local
cp -R "$prefix/share/omnirefl/tests" ./omnirefl-tests

mkdir build && cd build

cmake ../omnirefl-tests -GNinja

ctest . --output-on-failure
```

The copied tree also contains the runnable example and comprehensive guide
sources. For an unpacked package, set `prefix` to the unpacked install root.
The tests fetch their own test-only dependencies during CMake configuration. If
CMake does not find a non-standard install, pass
`-Domnirefl_DIR="$prefix/lib/cmake/omnirefl"`; on Windows this is usually needed
for an unpacked `.zip` package.

## Build and Develop Locally

The repository uses the
[`ghcr.io/sergio-eld/omnirefl-build-alpine-x86_64-musl-ucrt`](https://github.com/sergio-eld/omnirefl/pkgs/container/omnirefl-build-alpine-x86_64-musl-ucrt)
Alpine Docker image for local and CI builds. The image contains prebuilt
LLVM/Clang installs for both Linux and Windows targets; building that layer
from source can take close to an hour, so using the prepared image is the
simplest way to get reproducible local and CI builds. The image is defined by
[docker/build-alpine-x86_64-musl-ucrt.Dockerfile](docker/build-alpine-x86_64-musl-ucrt.Dockerfile).

The same Alpine image is used for the MinGW Windows cross-build. The Linux tool
build uses static musl linking, so the packaged executable has no runtime libc
dependency on the target Linux distribution. The Windows package targets UCRT;
no other runtime dependency is expected.

If configuring the build directly on the host instead of using the image, use a
C++23 compiler. As of now, the prepared build image uses GCC for the native
Linux tool build.

Build Linux and Windows packages:

```bash
docker compose run --rm build-linux
docker compose run --rm build-windows
```

The packages are written to `artifacts/packages/linux` and
`artifacts/packages/windows`. To work inside the same build image:

```bash
docker compose run --rm --entrypoint /bin/ash build-linux
```

## Examples

- [tests/tool/example/main.cpp](tests/tool/example/main.cpp) is the small
  runnable README example.
- [tests/tool/comprehensive_guide/comprehensive_guide.cpp](tests/tool/comprehensive_guide/comprehensive_guide.cpp)
  is the executable usage guide with C++20, compatibility, dependency,
  template, annotation, schema, and write examples. It is written for limited
  C++20 support and avoids `<concepts>` plus C++20 ranges algorithms.

## Tested Toolchains

Current package/install coverage is reported by the
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

## Continuous Benchmark

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
- (+) field `type_name()` without namespaces, including enclosing records when
  available
- (+) field `qualified_type_name()` preserves source declaration spelling when
  available
- (+) canonical metadata is available when the field type itself is reflectable
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
- (+) supported member alias dependencies: `error_type`, `first_type`,
  `key_type`, `mapped_type`, `second_type`, `type`, `value`, `value_type`
- (+) `std::pair` dependencies through `first_type` and `second_type`
- (+) supported template-pack dependencies for template names exactly `tuple`
  and `variant`
- (+) `std::` record types are ignored outside the supported protocol routes
- (+) recursive dependency walk through supported routes

### Serialization Completeness

- (+) reflect record field names
- (+) reflect record field types
- (+) reflect record field type names and qualified type names
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
- (+) best-effort tool diagnostics for invalid `reflected_call` arguments
  including pointers, arrays, fundamentals, standard-library records, and
  partial template specializations
- (+) best-effort tool diagnostics for reflection query instantiation outside a
  reflected scope

### Build/Release

- (+) generated-header reflection
- (+) CMake integration
- (+) packaged runnable example
- (+) packaged executable comprehensive guide
- (+) annotations enabled by default
- (+) annotations can be disabled with `--no-annotations` or CMake
  `NO_ANNOTATIONS`
- (+) `omnirefl -o <reflection.hpp> --source <source.cpp> -- <compiler command...>`
- (+) helper `ccdb_query <compile_commands.json> <source.cpp> [output-contains]`
  for CMake integration
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
- TODO(High): add a CLI option that treats skipped unsupported public bases as
  errors instead of warnings.
- TODO: consider extending reflected entity metadata for fundamental types.
  Currently canonical metadata is available for reflectable record/enum field
  types; fundamental field types are reported only through field declaration
  spelling.
- (-) refine the public interface
- (-) replace `reflected_call`
- (-) separate const and mutable public-field accessors in the public interface
- (-) recoverable reflection query/fallback branch for non-reflected types
- (-) type-erased field wrappers, likely short `field_t`-style names
- (-) refine the CLI interface
- (-) Unix-like invocation: `omnirefl -o <reflection.hpp> -MF <deps.d> -- <cc1 args...>`
- (-) split compiler-driver/compile-db args to cc1 mapping into a separate
  composable tool

## Might Be Considered Later

- (-) unnamed non-local types addressable from namespace scope via
  `decltype(symbol)`
- (-) unnamed non-local types addressable through function return type
- (-) other globally addressable unnamed cases

## Bug Reports

For tool crashes on Linux, please include the command line, stderr/stdout, the
input `.cpp`, the generated header if one was produced, and a backtrace.

```bash
# Enable core dumps for the current shell, then rerun the exact failing command.
ulimit -c unlimited
omnirefl -o out.omnirefl.hpp --source source.cpp -- <compiler command...>

# If your system writes core files into the working directory:
gdb --batch -ex "thread apply all bt full" ./omnirefl ./core > omnirefl.bt.txt

# If your system uses systemd-coredump:
coredumpctl gdb omnirefl --batch \
  -ex "thread apply all bt full" > omnirefl.bt.txt
```

If no core file is produced, check `cat /proc/sys/kernel/core_pattern`; some
systems route core dumps to a crash service instead of the current directory.
