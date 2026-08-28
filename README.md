# Omnirefl

<p align="center">
  <img src="omnirefl-banner.png" alt="Omnirefl" width="640">
  <br>
  <sub>Obligatory self-reflection meta joke.</sub>
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

# Reflection is not transitive: only this target's own C++ translation units are
# instrumented. Call omni_reflected_target for each target that should be
# reflected.
omni_reflected_target(example)
```

Instrumentation can also be triggered explicitly through `<target>.omni`
(`example.omni` for the `example` target):

```bash
cmake --build build -t example.omni
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

  const auto write = [](omni::binding auto b)
    // Generic lambdas used as reflected visitors must spell the return type.
    -> void {
      // Bindings and field bindings are trivial to copy.
      std::apply(
        [](omni::field_binding auto... field) -> void {
          const auto write = [](omni::field_binding auto field) -> void {
            constexpr std::string_view name = field.name();

            // value() is read-only; ref() exposes a writable reference.
            if constexpr ("foo"sv == name)
              field.ref() = 8;

            // operator* and operator-> are QoL accessors.
            if constexpr ("bar"sv == name)
              *field = "after";
          };

          (write(field), ...);
        },
        b.public_fields());
    };

  omni::reflected_call(write, value);

  std::cout << "after: foo=" << value.foo << " bar=" << value.bar << '\n';
}
```

The complete example is available in
[tests/tool/example](tests/tool/example). The
[comprehensive guide](tests/tool/comprehensive_guide/comprehensive_guide.cpp)
covers the remaining interface and compatibility features.

## Seamless Experience

1. Add `omni_reflected_target(...)` for the CMake target.
2. Use `omni::reflected_call(...)` where reflection is needed.

Everything else remains regular C++. Omnirefl discovers the argument types and
supported dependencies, then generates and force-includes their metadata. No
macros, compiler-specific UB, or manual regeneration are required.

## Supported Scope

Omnirefl focuses on POD-like records and enums.

- C++11 through C++23; C++20 concepts provide the most ergonomic interface.
- Named namespace-scope records and enums; namespace-scope unscoped enums
  require a fixed underlying type.
- Nested named records/enums of supported parents.
- Unconstrained primary record templates, including use as CRTP bases.
- Public data fields: names, type names, documentation, value retrieval,
  mutation of writable fields, reference access when safe,
  `has_value_access()`/`has_reference_access()` capability queries, and
  `is_const()`/`is_mutable()`/`is_volatile()`/`is_deprecated()` traits.
  Private/protected fields and member functions are not reflected.
- Enum documentation, plus enumerator names and values.

### Dependency Protocols

Additional reflected types are discovered through:

- public field types
- public bases and transitive public bases
- public fields of primary template records
- supported public member aliases:
  - `error_type`
  - `first_type`
  - `key_type`
  - `mapped_type`
  - `second_type`
  - `type`
  - `value`
  - `value_type`
- template-pack routes named `tuple` or `variant`

Standard-library record types are not traversed as reflectable records outside
those protocol routes.

### Limitations

- `reflected_call` is the instrumentation boundary. The visitor must be either
  a generic lambda or a type with a templated `operator()`. Its return type must
  not depend on instantiating the visitor body during the tool run; for lambdas,
  this means an explicit trailing return type, including `-> void`.
  Consequently, a lambda cannot currently return a type declared inside its
  body. `constexpr auto result = reflected_call(...)` is not supported: it
  forces evaluation and breaks that instrumentation boundary.
- `reflected_call` accepts reflected records and enums only. The caller must
  sanitize pointers, arrays, and compound inputs before the call; use
  `std::visit` or `mpark::visit` for variants. Compound types remain valid
  dependency routes as listed above. Invalid-input detection is best effort.
- Local and unnamed types are not supported as reflected roots.
- Records nested inside template records are not supported.
- Privately nested types cannot be reflected through a public field, including
  fields inherited from a public base.
- Records with direct or inherited virtual bases are not supported. They are
  rejected as `reflected_call` inputs and skipped with a warning when found as
  dependencies.
- Constrained primary record templates are not supported.
- Direct recursive `reflected_call` is not supported inside a reflected scope.
  A nested reflection call can only work if that reflected path was already
  instantiated independently.
- Reflection queries are valid only inside the reflected scope. The tool reports
  out-of-scope queries as errors on a best-effort basis.
- [Deprecated public fields can emit compiler deprecation diagnostics while
  their metadata is formed](tests/tool/regressions/deprecated_public_field.cpp),
  before `is_deprecated()` can filter them.
- Anonymous unions are not reflected correctly.
- Compiler-packed misaligned raw arrays have no safe whole-field accessor; use
  an aligned representation such as `std::array` when whole-field access is
  required.
- External pointees that are only forward-declared are not traversed.
- `omni_reflected_target` does not support OBJECT or INTERFACE libraries.
- The CMake wrapper instruments concrete, non-generated C++ translation units.
  Generated sources are skipped, source generator expressions are rejected,
  and C translation units are ignored. If no C++ source remains, reflection is
  skipped with a warning.

## Direct CLI Usage

`omni_reflected_target(...)` is a convenience wrapper; omnirefl itself does not
require CMake:

```bash
# Cosmopolitan packages use omnirefl on Unix and omnirefl.exe on Windows.
flags="-std=c++20 -I/path/to/omnirefl/include"
omnirefl -o example.omnirefl.hpp -c main.cpp -- c++ $flags
c++ $flags -include example.omnirefl.hpp main.cpp -o example && ./example
```

`-c` selects the instrumented source; compiler output options after `--` are
ignored. `ccdb_query` prints the matching command from a compilation database;
the optional final argument selects among commands by output-path substring:

```bash
ccdb_query build/compile_commands.json "$PWD/main.cpp" example.dir
```

## Install

Install options:

- Release archives:
  [browse published releases](https://github.com/sergio-eld/omnirefl/releases).
  Linux packages use `.deb` or `.tar.gz`, Windows packages use `.zip`, and the
  experimental Cosmopolitan `.tar.gz` package supports Linux, macOS, and
  Windows.
- Latest CI artifact:
  open the latest successful
  [`CI` workflow](https://github.com/sergio-eld/omnirefl/actions/workflows/ci.yml)
  run on `master` and download the package artifact for the required runtime
  and architecture.
- Build locally using the prepared Docker images; see
  [Build Packages Locally](#build-packages-locally).

Install a `.deb` normally. Unpack a `.tar.gz` or `.zip` archive and use its
`omnirefl-*` directory as the installation prefix.

## Packaged Tests and Examples

Packaged test/example sources are available under `share/omnirefl/tests`. Copy
them into a writable directory before configuring:

```bash
# Use /usr for a .deb, or the unpacked omnirefl-* directory for an archive.
prefix=/usr
cp -R "$prefix/share/omnirefl/tests" ./omnirefl-tests

mkdir build && cd build

cmake ../omnirefl-tests -GNinja \
  "-Domnirefl_DIR=$prefix/lib/cmake/omnirefl"

ctest --output-on-failure
```

On Windows, run from a Visual Studio Developer PowerShell so `cl.exe` is
configured:

```powershell
$prefix = "C:\path\to\omnirefl"
Copy-Item -Recurse "$prefix\share\omnirefl\tests" .\omnirefl-tests

New-Item -ItemType Directory build | Out-Null
Set-Location build

cmake ../omnirefl-tests -GNinja `
  "-Domnirefl_DIR=$prefix/lib/cmake/omnirefl"

ctest --output-on-failure
```

The tests fetch their own test-only dependencies during CMake configuration.

## Build Packages Locally

Docker Compose uses prepared, versioned build images. Rebuilding a complete
toolchain image locally can take close to an hour.

```bash
export PACKAGE_DIR=./artifacts/packages/current
docker compose run --rm build-musl
docker compose run --rm build-musl-aarch64
docker compose run --rm build-ucrt
docker compose run --rm build-cosmo
```

The Linux package test expects the matching musl archive and the universal
Cosmopolitan archive in `PACKAGE_DIR`; the commands above populate that
directory.

```bash
docker compose run --rm test-alpine
```

## Tested Platforms

The [CI workflow](https://github.com/sergio-eld/omnirefl/actions/workflows/ci.yml)
tests these package/platform combinations:

- Linux x86_64 musl and Cosmopolitan packages on Alpine and Ubuntu 18.04,
  20.04, and 22.04 with GCC and Clang.
- Linux AArch64 musl and Cosmopolitan packages on Alpine and Ubuntu 22.04 with
  GCC.
- Windows x86_64 UCRT and Cosmopolitan packages with MSVC, clang-cl, MSYS2 GCC,
  and MSYS2 Clang.
- The Cosmopolitan package on Intel and Apple Silicon macOS 15 and 26.

The Linux matrix also checks MinGW cross-compilation. Windows AArch64 packaging
is not currently supported.

## Is It Slow?

Omnirefl uses a Clang frontend action: it preprocesses the translation unit and
builds its AST, but does not perform object-code optimization or code
generation. Frontend work commonly accounts for around 30% of a complete object
build, although the ratio depends on the source, included headers, compiler, and
optimization level.

The [benchmark baseline](tests/tool/baseline_test.cpp) is intentionally large
enough to represent a meaningful translation unit and contains a reasonable
amount of ordinary and reflected code. As of this writing, its complete
omnirefl run takes about 20-25% of the subsequent Release object build in CI.
These percentages are relative to the object build alone: a five-second
compilation gains roughly one additional second when instrumentation runs,
which is generally negligible at whole-build scale.

Only instrumented targets pay this cost, and reflected translation units can be
isolated in dedicated targets. The impact is therefore most noticeable during
initial generation. Omnirefl emits dependency files for the source and all its
included headers, so Ninja reruns instrumentation only when one of those inputs
changes. See the [continuous benchmark](#continuous-benchmark) for measured
stages and regression tracking.

## Continuous Benchmark

CI benchmarks native musl and Cosmopolitan on Linux x86_64, plus Cosmopolitan
on Intel and Apple Silicon macOS. Benchmark inputs use `Release`;
distributable packages retain `RelWithDebInfo` for detached symbols.

Reports compare reflection and object-build wall time for `benchmark.baseline`
against the average of the last five stored runs.

## How It Works

`reflected_call` identifies root records and enums. Omnirefl walks their public
[dependency protocols](#dependency-protocols), then force-includes a generated
header before the translation unit.

The generated header does not need to include user declarations. It
forward-declares namespace-scope roots where C++ permits it; not every type can
be forward-declared (see [Limitations](#limitations)). Field access remains
dependent on a template parameter, delaying instantiation until the source
definition is available. Nested-type lookup uses the same mechanism through
SFINAE. A simplified generated shape is:

```cpp
namespace app {
struct root; // The definition may remain in the translation unit.
}

namespace omni {
namespace detail {

// Field accessors use T, so their instantiation is delayed until app::root is
// complete.
template <typename T>
struct _reflected<struct app::root, T> {
  // Metadata omitted.
};

// _wrt means "with respect to": its type is app::root, but remains
// syntactically dependent on T so nested-name lookup is delayed.
template <typename T>
struct _reflected<T,
  typename std::enable_if<
    std::is_same<T, typename _wrt<app::root, T>::type::nested>::value,
    T>::type> {
  // Metadata omitted.
};

} // namespace detail
} // namespace omni
```

This permits records declared directly in one translation unit to be reflected.
Without the delayed specializations, users would need separate declaration
headers and a manually ordered wrapper around generated metadata. That wrapper
would still need a policy for which reachable types to reflect; Omnirefl derives
roots from `reflected_call` and dependencies from the protocols above, enabling
a seamless experience.

## Troubleshooting and Bug Reports

Language servers can report temporary diagnostics because reflected translation
units depend on a force-included generated header. Build the affected source or
refresh it through the `<target>.omni` target.

Invalid C++ in an instrumented translation unit is reported as a Clang error.
Compiler warnings are not reported by omnirefl.

Report defects through
[GitHub Issues](https://github.com/sergio-eld/omnirefl/issues). For tool crashes
on Linux, please include the command line, stderr/stdout, the input `.cpp`, the
generated header if one was produced, and a backtrace.

```bash
# Enable core dumps for the current shell, then rerun the exact failing command.
binary=./omnirefl # Use ./omnirefl.exe for the Cosmopolitan APE payload.
ulimit -c unlimited
"$binary" -o out.omnirefl.hpp -c source.cpp -- <compiler command...>

# If your system writes core files into the working directory:
gdb --batch -ex "thread apply all bt full" "$binary" ./core > omnirefl.bt.txt

# If your system uses systemd-coredump:
coredumpctl --output=omnirefl.core dump "$(basename "$binary")"
gdb --batch -ex "thread apply all bt full" \
  "$binary" omnirefl.core > omnirefl.bt.txt
```

If no core file is produced, check `cat /proc/sys/kernel/core_pattern`; some
systems route core dumps to a crash service instead of the current directory.

## License

Omnirefl is available under the [MIT License](LICENSE).
