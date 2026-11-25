# Cached builds

## ccache

Use CMake's compiler launcher when configuring your project:

```sh
cmake -S . -B build -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build build --parallel
ccache --show-stats
```

For a single existing target instead:

```cmake
set_property(TARGET my_target PROPERTY CXX_COMPILER_LAUNCHER ccache)
```

The [launcher setting][launcher] applies to compilation. It caches object
builds, including those using generated reflection headers; it does not cache
Omnirefl's instrumentation step. A clean build can therefore rerun reflection
while obtaining compiler-cache hits.

## Precompiled headers

Use `target_precompile_headers` normally and start with ccache's defaults.
Some PCH operations may bypass the cache; this is not a compilation failure.

[Ccache's PCH configuration][pch] requires opt-in settings such as
`sloppiness=pch_defines,time_macros`; Clang also needs
`-Xclang -fno-pch-timestamp`. These relax macro and timestamp checks, so
Omnirefl does not enable them automatically. Check the requirements for your
ccache and compiler versions before enabling PCH caching.

Clang 6 with ccache 3.4.1 reused a stale PCH after a header change when those
opt-ins were enabled with direct mode. A minimal project without Omnirefl
reproduced the wrong result. Default settings avoided that caching path.
Disabling direct mode (`direct_mode=false`) with the PCH settings passed the
same mutation checks, but retains their macro/time trade-offs.

## Checked configurations

| Compiler | ccache | Checks |
| --- | --- | --- |
| GCC 15.2 | 4.12.1 | Native execution, plain and PCH builds |
| Clang 6 | 3.4.1 | Native execution, plain and PCH builds |
| MinGW 7.3 | 3.4.1 | Windows-targeted compilation and linking |

Checks covered cache hits after clean rebuilds, changed headers, compile
definitions, C++ standards, and reversion. Shared sources, OBJECT libraries,
and uninstrumented targets were included. Native MSVC/clang-cl ccache behavior
has not been checked.

[launcher]: https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_COMPILER_LAUNCHER.html
[pch]: https://ccache.dev/manual/4.12.1.html#_precompiled_headers
