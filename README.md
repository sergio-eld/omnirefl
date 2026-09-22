<!-- pages:introduction:start -->
# Omnirefl

A C++ reflection tool built for a seamless experience without macros* or UB.<br>
<sub>* Some compatibility QoL before C++17/20 uses macros.</sub>

<p align="center">
  <img src="omnirefl-banner.png" alt="Meme comparing Omnirefl AST parsing and template machinery with languages that have built-in reflection" width="640">
  <br>
  <sub>Obligatory self-reflection meta joke.</sub>
</p>
<!-- pages:introduction:end -->

<!-- pages:sneak-peek:start -->
## Sneak Peek

The [complete runnable example](examples/sneak_peek/main.cpp) implements the
visitor and shows how to write reflected fields. Its setup and model are below:

```cpp
// One .cpp; no declaration headers, metadata files, or reflection macros.

// Example visitor that prints reflected records during a depth-first walk
// of type dependencies. The struct keeps this logic below; an inline
// lambda would also work.
struct print_reachable_records {
  template <omni::record_meta Root>
  void operator()(Root) const;
};

template <typename T>
void print_reflection() {
  // `reflected_call` makes metadata for `T` and its type dependencies available
  // inside the visitor's `operator()` and the functions it calls.
  omni::reflected_call(print_reachable_records{}, omni::type<T>);
}

// Mutating example; its implementation showcases several reflected arguments
// in one `reflected_call`.
template <typename... Values>
void update_fields(Values *...values);

// Omnirefl reflects globally accessible, named records (structs, classes, or
// unions) and enums on demand.

namespace oceanic {
/// Declaration comments are available as reflection metadata.
struct vessel { //< Discovered as the `mapped_type` of `fleet<T>::vessels`.
  // Nested structs are supported recursively inside non-template records.
  struct position { //< Discovered via the `location` field.
    using degrees = double;

    degrees latitude = 0;
    degrees longitude = 0;
  };

  position location;

  // Reflected field properties are queryable.
  mutable std::string name = "before"; //< `.is_mutable()` is true.

  // Public non-static methods are reflected. Their parameter and return types
  // join dependency discovery. Overloaded methods are skipped with a warning.
  // Doxygen parameter descriptions, such as `@param` or `\param[in]`, are
  // available through parameter metadata.

  /** Check proximity.
   * @param other Other position.
   * @return Whether the positions are near.
   */
  bool is_near(position other) const {
    return location.latitude == other.latitude
      && location.longitude == other.longitude;
  }
};

/** Measure a distance.
 * @param scale Multiplier.
 * @return The scaled distance.
 */
double distance(vessel, double scale) {
  return scale;
}

// Primary templates are supported; names appear without template arguments.
template <typename T>
struct fleet { //< Discovered as an `omni::type_t` argument to `reflected_call`.
  // Unsupported nested records emit a warning if discovered as dependencies.
  struct not_supported {};

  // Free functions are reflected only when discovered through a record field
  // using the special `function_t` tag.
  omni::function_t<distance> measure;

  // Standard-library types are not reflected, but their dependency protocols
  // still apply. The specialization for `T` is discovered via `mapped_type`.
  std::map<std::string, T> vessels;
};

struct telemetry { //< Discovered as a value argument to `reflected_call`.
  // Writing bit-fields without UB is supported via `.set_value()`.
  mutable unsigned depth : 10 = 42;
  const unsigned sensor = 108; //< `.is_const()` is true.

  // Only public fields are reflected.
  private:
  [[maybe_unused]] unsigned john_cena = 49; //< can't see
};

} // namespace oceanic
```

Minimal CMake setup:

```cmake
# 3.18.2 is the current project floor for CMake APIs used by the package and
# reflected target integration.
cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(example LANGUAGES CXX)

find_package(omnirefl CONFIG REQUIRED)

add_executable(example sneak_peek/main.cpp)
set_property(TARGET example PROPERTY CXX_STANDARD 23)

# Reflection is not transitive: only this target's own C++ translation units are
# instrumented. Call omni_reflected_target for each target that should be
# reflected.
omni_reflected_target(example)
```

Build the example:

```bash
# Builds rerun instrumentation automatically for changed inputs.
# To trigger it manually:
# cmake --build build --target example.omni
cmake --build build --target example
```

Run it:

```console
$ ./build/example
// Primary templates are supported; names appear without template arguments.
oceanic::fleet {
  measure(vessel, double scale [Multiplier.]) -> double; // Measure a distance.
  // Standard-library types are not reflected, but their dependency protocols
  // still apply. The specialization for `T` is discovered via `mapped_type`.
  vessels: map<std::string, T>;
}
// Declaration comments are available as reflection metadata.
oceanic::vessel {
  location: vessel::position (resolves to vessel::position);
  // `.is_mutable()` is true.
  name: string = before;
  is_near(vessel::position other [Other position.]) -> bool; // Check proximity.
}
// Nested structs are supported recursively inside non-template records.
oceanic::vessel::position {
  latitude: vessel::position::degrees = 0;
  longitude: vessel::position::degrees = 0;
}
before: depth=42 sensor=108 name=before
after:  depth=815 sensor=108 name=oceanic
```

This example is included in the installed package; see
[Examples, Tests and Benchmarks](#examples-tests-and-benchmarks).

The [comprehensive guide](tests/tool/comprehensive_guide/comprehensive_guide.cpp)
covers the remaining interface and compatibility features.
<!-- pages:sneak-peek:end -->

<!-- pages:experience:start -->
## Seamless Experience

1. Add `omni_reflected_target(...)` for the CMake target.
2. Use `omni::reflected_call(...)` where reflection is needed.

Everything else remains regular C++. Omnirefl discovers the argument types and
supported dependencies, then generates and force-includes their metadata. No
macros, compiler-specific UB, or manual regeneration are required.

Types can be declared and reflected directly in the same `.cpp`. No dedicated
declaration headers, schemas, annotations, or checked-in metadata files are
required; generated metadata remains a build artifact.
<!-- pages:experience:end -->

<!-- pages:status:start -->
## Status

Omnirefl is under active testing and interface polishing. Version 0.1.0 is
planned after the initial set of extensions is complete. Until then, interfaces
and package layout may change without compatibility guarantees. Release notes
aim to call out every breaking interface change.
<!-- pages:status:end -->

## Functional Utilities

`<omnirefl/functional.hpp>` provides compositors such as `each`, `filter`,
`map`, and `foldl`, including chainable forms:

```cpp
const auto result = tuple
  | omni::fn::filter(predicate)
  | omni::fn::map(transform)
  | omni::fn::foldl(combine, initial);
```

See the [functional tests](tests/functional/test.cpp) for detailed examples.

## Reflection Utilities

The experimental `omni::refl` utilities are implemented through code generated
for each reflected type and are available only within reflected scopes:

- `aggregate_into<T>(fields)` shallowly constructs a reflected aggregate by
  matching field bindings by name. All destination public fields must be
  present and constructible; additional source fields are ignored. Generated
  support currently requires an aggregate record without bases or anonymous
  aggregate members, and excludes unions. See the
  [example](tests/tool/aggregate_into/test.cpp).

<!-- pages:scope:start -->
## Supported Scope

Omnirefl reflects the public data surface of named C++ records and enums (see
[Limitations](#limitations)).

- **Language:** C++11 through C++23; C++20 concepts provide the most ergonomic
  interface.
- **Reflectable declarations:**
  - named namespace-scope records (structs, classes, and unions) and enums
  - nested named records and enums inside non-template records
  - unconstrained primary record templates with type, non-type, and
    template-template parameters, including type packs and CRTP bases
  - non-aggregate records and records without a default constructor, when
    supplied as existing objects or queried through `omni::type<T>`
- **Type metadata:**
  - type names with and without enclosing namespace qualification, entity kind,
    and documentation extracted from Doxygen-style leading and trailing
    comments: `///`, `//!`, `/** */`, `/*! */`, `///<`, and `//!<`
  - qualified names retain enclosing record and namespace identifiers,
    including those of inline namespaces
  - records additionally expose `has_bases()` and generated
    `is_aggregatable()` capability queries
- **Public field metadata and access:**
  - an ordered tuple of public non-static fields, including fields inherited
    transitively through public bases; hidden and ambiguous inherited fields
    are omitted
  - field name, type names preserving declaration spelling such as alias
    templates and `decltype`, with and without enclosing namespace
    qualification, an index local to the declaring record, documentation, and
    const/mutable/volatile/deprecated traits
  - default member initializer presence and best-effort access to values
    accepted for reproduction in generated metadata; skipped values emit a
    warning and remain distinguishable from fields without an initializer
  - read access, moving through `std::move(field).value()`, writable-field
    assignment, and safe reference, dereference, and member access
  - value/reference capability queries for generic field handling
  - bitfield and misaligned packed scalar members remain readable; writable
    members remain assignable but do not expose references
  - private/protected fields, static fields, and fields inherited through
    non-public bases are omitted
- **Public function metadata:**
  - non-static public member functions without parameter packs; overloaded
    methods are skipped with a warning, while conversion operators and
    immediate functions are omitted
  - fixed `function_t` fields as special field members for free or static
    functions; dependent function-tag fields in primary templates remain
    ordinary fields without function metadata
  - source name, documentation, pointer, arity, parameter metadata, and return
    metadata; parameter and return metadata include source-spelled type names
    and tagged documentation
- **Enum metadata:** enumerator names and values in declaration order.
- **Invocation and bindings:**
  - `reflected_call` value arguments produce non-owning bindings;
    `omni::type<T>` requests metadata without constructing `T`
  - record and enum metadata expose their domain type through `reflected_type`;
    the generated metadata template argument is intentionally opaque
  - field bindings expose the cv-qualified bound record type separately from
    opaque field metadata
  - one callable can receive multiple value and type arguments
  - value bindings preserve const/volatile and lvalue/rvalue qualification;
    callable value and reference returns are preserved
  - `omni::reflected(...)` and `is_reflected<T>` query generated dependency
    metadata from inside the callable

<!-- pages:scope:end -->

<!-- pages:dependency-protocols:start -->
### Dependency Protocols

Additional reflected types are discovered through:

- public field types
- public bases and transitive public bases
- public fields of primary template records
- parameter and return types of supported function metadata
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

Supported public routes may expose otherwise non-public nested dependencies.

Standard-library record types are not traversed as reflectable records outside
those protocol routes.

<!-- pages:dependency-protocols:end -->

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

<!-- pages:install:start -->
## Install

- **Release archives:**<br>
  [Latest release](https://github.com/sergio-eld/omnirefl/releases/latest) or
  [all releases](https://github.com/sergio-eld/omnirefl/releases). Linux
  packages use `.deb` or `.tar.gz`; Windows packages use `.zip`. The
  experimental Cosmopolitan `.tar.gz` package supports Linux, macOS, and
  Windows.
- **Latest CI artifact (if available):**<br>
  Open the latest successful
  [`CI` workflow](https://github.com/sergio-eld/omnirefl/actions/workflows/ci.yml)
  run on `master` and download the package artifact for the required runtime
  and architecture. Artifacts are temporary; cancelled or partially rerun
  workflows and artifact expiration may leave no downloadable package.
- **Build locally:**<br>
  Use the prepared Docker images; see
  [Build Packages Locally](#build-packages-locally).

Install a `.deb` normally. Unpack a `.tar.gz` or `.zip` archive and use its
`omnirefl-*` directory as the installation prefix.
<!-- pages:install:end -->

<!-- pages:examples-tests-and-benchmarks:start -->
## Examples, Tests and Benchmarks

- `share/omnirefl/examples`
- `share/omnirefl/tests` — tests and benchmarks

The example requires a configured C++23 toolchain. Download and unpack the
Cosmopolitan `.tar.gz` from the
[latest release](https://github.com/sergio-eld/omnirefl/releases/latest) in
your working directory. Set `prefix` to the extracted `omnirefl-*` directory,
then run:

```bash
# Uncomment if Omnirefl was installed system-wide from a Debian package.
# prefix=/usr

cp -r "$prefix/share/omnirefl/examples" ./omnirefl-examples
cd omnirefl-examples
mkdir build
cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo "-DCMAKE_PREFIX_PATH=$prefix"
cmake --build .
./example
```

The C++23 requirement applies only to the example. To run the packaged tests,
copy `tests`; add `-DENABLE_BENCH=ON` while configuring to include benchmarks.
After building, run `ctest --output-on-failure`.
<!-- pages:examples-tests-and-benchmarks:end -->

<!-- pages:limitations:start -->
## Limitations

Several declaration-shape constraints below follow from the generated-header
model: reflected types must be nameable before their source declarations. See
[How It Works](#how-it-works).

- `reflected_call` is the instrumentation boundary. The callable must be either
  a generic lambda or a type with a templated `operator()`. Its return type must
  not depend on instantiating the callable body during the tool run; for lambdas,
  this means an explicit trailing return type, including `-> void`.
  Consequently, a lambda cannot currently return a type declared inside its
  body. `constexpr auto result = reflected_call(...)` is not supported: it
  forces evaluation and breaks that instrumentation boundary.
- `reflected_call` accepts reflected records and enums only. The caller must
  convert or dispatch other top-level shapes before the call; use `std::visit`
  or `mpark::visit` for variants. Scalars, pointers, raw arrays,
  standard-library records, and compound types are not accepted directly.
  Compound types remain valid dependency routes as listed above. Invalid-input
  detection is best effort.
- A reflected root must be complete and defined before its `reflected_call`.
- `default_value()` is generated only when the initializer appears safe to
  copy into generated metadata. Detection is conservative and best effort.
  Function calls, declaration references, `this`, macros, and dependent
  expressions are skipped with a warning. `has_default_member_initializer()`
  still reports the declaration, while `has_default_value_access()` reports
  whether its value is available.
- Incomplete dependency types are skipped with a warning. A class-template
  dependency is also skipped when instantiating it would require an incomplete
  type argument.
- Local and unnamed types are not supported as reflected roots.
- Namespace-scope unscoped enums require a fixed underlying type so the
  generated header can forward-declare them.
- Records nested inside template records are not supported.
- Public access paths to non-public nested dependencies are not preserved when
  the exposing field is inherited from a public base. A public nested type
  inside a private enclosing record is also not currently nameable.
- Records with direct or inherited virtual bases are not supported. They are
  rejected as `reflected_call` inputs and skipped with a warning when found as
  dependencies.
- Constrained primary record templates and explicit or partial record-template
  specializations are not supported. `reflected_call` rejects them as roots;
  explicit or partial specialization dependencies are skipped with a warning.
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
- Pointer/reference pointees and raw-array element types are not dependency
  routes, regardless of whether their definitions are visible.
- Standard-library public bases are ignored. Other unsupported public bases are
  skipped with a warning, and their inherited fields are omitted.
- `omni_reflected_target` does not support OBJECT or INTERFACE libraries.
- The CMake wrapper instruments concrete, non-generated C++ translation units.
  Generated sources are skipped, source generator expressions are rejected,
  and C translation units are ignored. If no C++ source remains, reflection is
  skipped with a warning.
<!-- pages:limitations:end -->

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

<!-- pages:performance:start -->
## Is It Slow?

Omnirefl uses a Clang frontend action: it preprocesses the translation unit and
builds its AST, but does not perform object-code optimization or code
generation. The overhead target is roughly the frontend portion of a complete
object build: about 30% as an order-of-magnitude expectation. The actual ratio
depends on the source, included headers, compiler, and optimization level.

The packaged [benchmark baseline](tests/tool/baseline_test.cpp) is intentionally
large enough to represent a meaningful translation unit and contains a
reasonable amount of ordinary and reflected code. CI records its reflection and
subsequent Release object-build times across benchmarked platforms. See the
[continuous benchmark](#continuous-benchmark) and
[workflow history](https://github.com/sergio-eld/omnirefl/actions/workflows/ci.yml)
for observed results.

Only instrumented targets pay this cost, and reflected translation units can be
isolated in dedicated targets. The impact is therefore most noticeable during
initial generation. Omnirefl emits dependency files for the source and all its
included headers, so Ninja reruns instrumentation only when one of those inputs
changes.
<!-- pages:performance:end -->

## Continuous Benchmark

CI benchmarks native musl and Cosmopolitan on Linux x86_64, plus Cosmopolitan
on Intel and Apple Silicon macOS. Benchmark inputs use `Release`;
distributable packages retain `RelWithDebInfo` for detached symbols.

Reports compare reflection and object-build wall time for `benchmark.baseline`
against the average of the last five stored runs.

<!-- pages:how-it-works:start -->
## How It Works

`reflected_call` identifies root records and enums. Omnirefl walks their public
[dependency protocols](#dependency-protocols), then force-includes a generated
header before the translation unit.

The generated header is an internal, per-translation-unit build artifact. It is
not intended to be installed or published as a reusable interface: its metadata
reflects the exact compiler invocation, including preprocessor definitions,
language and target flags, and include paths. The same source may therefore
produce different metadata in another target or project. The header does not
`#include` user declaration headers or reproduce their definitions.

> Earlier iterations attempted to reconstruct the required user includes, but
> that becomes a separate build-integration problem: a declaration may live
> only in a `.cpp`, a third-party header's supported include path may differ
> from its filesystem path, and project headers may rely on transitive includes
> or a particular include order.

The current design avoids guessing. It forward-declares namespace-scope roots
where C++ permits it; not every type can be forward-declared (see
[Limitations](#limitations)). Field access remains dependent on a template
parameter, delaying instantiation until the source definition is available.
Nested-type lookup uses the same mechanism through SFINAE. A simplified
generated shape is:

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
  // Internal discovery hook for the reflected C++ type.
  using type = T;

  // Metadata omitted.
};

// _wrt means "with respect to": its type is app::root, but remains
// syntactically dependent on T so nested-name lookup is delayed.
template <typename T>
struct _reflected<T,
  typename std::enable_if<
    std::is_same<T, typename _wrt<app::root, T>::type::nested>::value,
    T>::type> {
  // Internal discovery hook for the reflected C++ type.
  using type = T;

  // Metadata omitted.
};

} // namespace detail
} // namespace omni
```

This model also defines the declaration boundary. Generated code can reproduce
ordinary record and enum forward declarations and defer nested lookup, but it
cannot safely recreate local or unnamed types, non-forward-declarable enums,
or records nested in template records. Constrained primary templates and
explicit or partial specializations are also unsupported.

Functions have no equivalent general forward-declaration strategy: reproducing
a declaration requires its parameter types and overload identity, which may not
be nameable before the source declaration. Therefore `function_t` is not a
standalone reflection root. The first implementation discovers it only as a
special field member of a reflected record, where generated access can remain
dependent on the owning record. The function's parameter and return types then
enter dependency discovery on a best-effort basis.
<!-- pages:how-it-works:end -->

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
