# Instrumentation header reuse

A tool-owned Clang PCH makes the first instrumentation of a different `.cpp`
faster when it shares a suitable header prefix. The complete GCC package
workflow measured a 3.2% improvement with a multi-source-only policy in one
comparison; broad enablement was 13.5% slower than stock CI. The prototype
reuses the existing `CompilerInvocation -> ASTUnit` pipeline.

Investigation: 2026-11-17, branch `investigate/instrumentation-pch`, based on
master `1d72e76`. This branch is an experiment, not a release-ready cache.

## Results

Three rounds, alternating baseline/cache order. Each cache round starts with
no tool PCH. Each source has a separate Omnirefl process. The second source
has never contributed to that cache: only the first source prepared it.
Operating-system file caches were warm. Linux runs were pinned to one CPU.
These are instrumentation timings, not compilation or whole-CI timings.

The core sequence uses these existing files, in order:

1. `static_members_test.cpp`
2. `function_metadata_test.cpp`
3. `interface_test.cpp`
4. `dependent_types_test.cpp`
5. `binding_metadata_test.cpp`

They contain approximately 5,000 lines and run 247 tests. The JSON sequence is
`to_record.cpp`, `to_enum.cpp`, `dispatch.cpp`, running 107 tests. No synthetic
translation units are used for the comparisons.

| Consumer / prefix | Second file (s) | Whole sequence (s) | Less time |
| --- | --- | --- | --- |
| GCC / standard | 2.283 -> 1.697 | 12.612 -> 10.711 | 15.1% |
| GCC / GTest | 2.341 -> 1.239 | 12.789 -> 9.196 | 28.1% |
| GCC / GTest + fn | 2.333 -> 1.150 | 12.581 -> 9.011 | 28.4% |
| GCC / JSON | 4.150 -> 2.197 | 13.667 -> 10.877 | 20.4% |
| Clang / GTest + fn | 2.421 -> 1.115 | 12.511 -> 8.735 | 30.2% |
| MSVC / GTest + fn | 1.293 -> 0.878 | 7.349 -> 6.403 | 12.9% |

Versions: GCC 15.2, Clang 21.1, MSVC 19.44. `fn` means functional.hpp; JSON
means json_match.hpp. All figures are medians. Full sequences include prefix
creation in the first source's process. The standard-header group is
`<memory>`, `<string>`, `<tuple>`, `<type_traits>`, `<utility>`, `<vector>`.

GCC/Clang consumers ran in Linux Docker. MSVC used the Windows Docker engine
(`docker.exe --context windows`) and the existing MSVC image.
Packages use embedded LLVM 22.1.8. Consumer compiler names identify their
compile-command formats; instrumentation always uses embedded Clang.

Windows wall times include the packaged CMake/ccdb_query bridge. Summing just
the tool's measured phases gives 7.142 -> 6.194 s for the full sequence and
1.253 -> 0.835 s for the second file. Compare results within a platform;
Windows and Linux use different standard-library headers and environments.

## Complete package workload (GCC)

The full comparison follows the package CI workload: extracted tracked extras,
source/build paths containing spaces, Release mode, examples and extensions
included, benchmarks disabled, C++11/14/17/20/23, and eight build jobs. All 249
enabled CTest cases run; 28 existing cases remain disabled. Dependency sources
are already fetched. Tool compilation, extraction, and downloads are outside
the timers; configuration is measured separately and included in totals.

Unlike the per-file medians above, these are single full runs per policy.

| Policy | Build (s) | CTest (s) | Total (s) |
| --- | --- | --- | --- |
| Stock CI, no added PCH | 320.9 | 290.4 | 612.7 |
| PCH on GTest-linked targets, cache off | 332.2 | 361.4 | 695.1 |
| Same PCH policy, cache on | 315.8 | 378.3 | 695.6 |
| PCH + cache on multi-source targets only | 311.9 | 279.5 | 592.9 |

The isolated cache comparison saves 16.4 s during the build but adds 16.9 s to
CTest: effectively no overall change. Broad cached-PCH enablement is 13.5%
slower than the stock no-PCH workload. CTest includes deliberately forced
`.omni` regeneration and subsequent recompilation, not just test execution.

The broad policy covers 70 GTest-linked targets, including one-source tests.
The initial cached build prepares 25 prefixes and reuses them 125 times.
Targets with a single compatible source still pay preparation without a
second source to amortize it. A configuration-only targeted policy limits PCH
to targets with at least two C++ sources (15 targets). It finishes 19.8 s, or
3.2%, faster than stock CI in this single comparison. That is a modest gain,
not the 20–30% suggested by looking only at selected instrumentation calls.

For the matched-PCH build, summed process wall times show where the gains go:

| Instrumentation phase | Cache off (s) | Cache on (s) |
| --- | --- | --- |
| Parse AST | 695.4 | 361.8 |
| Fold matches | 88.0 | 151.6 |
| Prefix preparation / validation / lock waits | 0 | 157.9 |

These sums include concurrent processes and waiting; they are not CPU times
or percentages of total build wall time.

To reproduce the complete comparison, use the shipped extras archive rather
than an untracked checkout, which may contain local scratch projects. The
manual policy hook requires CMake 3.24 or newer:

```sh
python3 internal/instrumentation-pch/full_suite.py \
  --extras /path/to/extracted/omnirefl-extras \
  --package /path/to/extracted/package \
  --root /tmp/pch-full-suite --jobs 8 \
  --dependencies /path/to/cached/_deps
```

The script runs broad-PCH/cache-off, broad-PCH/cache-on, stock CI, and targeted
multi-source policies sequentially, with separate clean build directories.

### Per-file phase and memory costs

For GCC's second core file with GTest + functional.hpp:

| Phase | Baseline | Cached |
| --- | --- | --- |
| Parse AST | 2.018 s | 0.519 s |
| Fold matches | 0.287 s | 0.587 s |
| Prefix validation / loading setup | 0 | 0.034 s |

The fold gets slower; it now pays for deserializing header declarations.
The total still improves. Clang documents its [lazy AST loading][clang-pch];
these measurements do not imply that the matcher overhead disappeared.

Peak process RSS for that workload changes from 223 to 292 MiB on the first
file, and from 222 to 210 MiB on the second. JSON changes from 324 to 424 MiB
on the first file, and from 300 to 278 MiB on the second. Preparation is not
free, especially for a target with only one source.

PCH sizes are approximately 14 MiB for standard headers, 22 MiB for GTest,
23 MiB for GTest + functional.hpp, and 30 MiB for json_match.hpp.

## Approaches checked

- **CMake-generated prefix:** reuse the user's `target_precompile_headers`
  header list through `cmake_pch.hxx`, rebuilding it with Omnirefl's own
  normalized invocation. Standard-only, GTest, and GTest + functional.hpp
  subsets were compared.
- **Explicit tool prefix:** cache the existing JSON tests' first shared
  header, json_match.hpp. The application need not use PCH. Each selected
  source already includes this header first; this experiment does not justify
  hoisting arbitrary includes or source-local macro definitions.
- **Compiler PCH artifacts:** retain the user's normal PCH setup, but do not
  import GCC/MSVC/another Clang build's binary artifact into embedded Clang.
  GCC's `.gch` was explicitly rejected as a non-Clang PCH by the embedded
  driver. The application and instrumentation need separate PCH artifacts.

`.dep` files still handle unchanged translation units. This is header-state
reuse across different translation units, not another unchanged-file cache.
Only one prefix is loaded per invocation; arbitrary cached header subsets
cannot simply be merged. Clang's [PCH model][clang-pch] requires a prefix at
the beginning of the translation unit.

## Prototype changes

`main.cpp` gains a stage between normalization and AST parsing:

1. Clone the normalized invocation, making the chosen header its input.
2. Remove the original source's debug main-file name from cache identity by
   replacing it with the prefix name.
3. Hash the normalized prefix invocation and LLVM version.
4. Validate all recorded prefix dependencies, including content and mtime.
5. Build a prefix with `GeneratePCHAction` on a miss; load it through
   `ImplicitPCHInclude` for the remaining source.
6. Merge prefix dependencies into the generated `.d` file. PCH loading does
   not replay the original include callbacks.

CMake opts in with `OMNIREFL_PCH_CACHE_DIR`. The bridge recognizes CMake's
wrapper only when the target's PCH property is enabled. This also recognizes
headers inherited from an interface library. Source files skipping PCH have
no wrapper argument and do not enter this path.

This metadata matters for GCC: its command uses ordinary `-include`, and on a
cold build instrumentation runs before GCC creates the `.gch`. Neither the
flag alone nor the binary artifact's existence identifies PCH intent.
[CMake documents the wrapper and force-include mechanism][cmake-pch].

MSVC requires one extra normalization: remove the private per-source
`OMNIREFL_GENERATED_HEADER` macro after excluding its dispatcher. Otherwise
each source's generated-header path unnecessarily creates a different prefix
key. Ordinary user macros remain part of cache identity.

Parallel first users coordinate through LLVM's lock utility. Each generated
PCH has a unique filename so later rebuilds do not overwrite an AST file
another process may still map. Manifests record the expected dependency count
so a truncated manifest cannot validate only its remaining subset.

## Correctness checks

- Clean package-consumer builds: 247 existing tests pass under GCC, Clang,
  and MSVC, with automatic CMake prefix selection and parallel generation.
- Existing JSON files: all 107 tests pass with headers generated using the
  explicit json_match.hpp prefix.
- Existing incremental-build fixtures: direct and transitive header changes
  regenerate metadata under GCC and MSVC, and the executable sees the added
  field. These fixtures also exercise C++11 mode under GCC.
- Touching an unchanged header rebuilds the prefix successfully under GCC and
  MSVC. Content-only validation initially failed because Clang checks mtimes.
- An intentionally truncated manifest rebuilds successfully and retains the
  correct reflected field count.
- An unchanged CMake rebuild does no work.
- Generated contents match after excluding the generated timestamp and
  declaration ordering. Declaration ordering can differ after PCH loading;
  the compiled tests check actual behavior rather than treating sorted text
  as a substitute for compilation.

## Existing GCC application-PCH problem

The current GCC CMake integration force-includes the generated reflection
header before the user's PCH wrapper. GCC therefore sees C++ tokens before
its PCH and silently parses the wrapper normally. A successful build alone
does not prove that the application used its PCH.

Using an existing metadata test's real compile command with `-H` confirmed:

```text
Current ordering:      .../cmake_pch.hxx
Reflection after PCH:  ! .../cmake_pch.hxx.gch
```

Both orders compile. GCC documents that [a PCH must precede C tokens][gcc-pch].
The instrumentation-prefix gains above are independent of this existing
application-compiler issue. Fixing the CMake include order needs to preserve
per-source generated headers, PCH reuse, and `SKIP_PRECOMPILE_HEADERS`; this
investigation does not replace that logic with a partial ordering fix.
MSVC's existing after-PCH dispatcher and Clang's PCH path both built and ran
successfully.

## Reproduction

The adjacent CMake project uses existing repository test sources and an
installed package. It fetches only test dependencies; existing FetchContent
source directories can be supplied to avoid downloads.

Build this branch's tool/package using the normal configured build, then
extract the archive. For Linux:

```sh
cmake -S internal/instrumentation-pch -B /tmp/pch-consumer -G Ninja \
  -DCMAKE_PREFIX_PATH=/path/to/extracted/package \
  -DCMAKE_BUILD_TYPE=Debug -DPCH_PROFILE=functional \
  -DOMNIREFL_PCH_CACHE_DIR=/tmp/pch-consumer/cache

python3 internal/instrumentation-pch/benchmark.py \
  --build /tmp/pch-consumer --package /path/to/extracted/package \
  --output /tmp/pch-measurements --rounds 3

cmake --build /tmp/pch-consumer --target pch_research.test --parallel 3
/tmp/pch-consumer/pch_research_test
```

Other profiles: `standard`, `gtest`, `none`. For JSON, configure
`-DWORKLOAD=json -DPCH_PROFILE=none` and supply the existing header explicitly:

```sh
python3 internal/instrumentation-pch/benchmark.py \
  --build /tmp/pch-json-consumer --package /path/to/extracted/package \
  --output /tmp/pch-json-measurements --rounds 3 \
  --prefix "$PWD/extras/extensions/serialization/tests/json_match.hpp"
```

Inside Windows Docker, initialize the image's MSVC developer environment,
configure the same project with Ninja and the Windows package, then use:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File C:\source\internal\instrumentation-pch\benchmark.ps1 `
  -Build C:\pch-consumer -Package C:\package `
  -Output C:\pch-measurements -Rounds 3
```

The script preserves the package's compile-command tokenization through its
CMake bridge.

Measured summaries are retained in [results.json](results.json). Raw
measurements and logs are saved under `/tmp/omnirefl-pch-results`. Container
build directories and the reproduction scripts retain the command details.

## Before production use

The result supports targeted, explicit prefix reuse with small pipeline
changes. The full-suite result does not justify broad default enablement.
Single-source target costs and the existing GCC application-PCH ordering
problem should be addressed before expecting substantial CI-wide gains.
Corrupt-PCH fallback, cache pruning, interrupted publication, stronger cache
format/build identity, and include-search changes need deliberate handling.
Working-directory and relative-path compatibility also need a dedicated audit
before sharing a cache beyond one configured build. macOS and Cosmopolitan's
Windows filesystem adapter were not tested here; the Windows check uses the
native UCRT package.

[clang-pch]: https://clang.llvm.org/docs/PCHInternals.html
[cmake-pch]:
  https://cmake.org/cmake/help/latest/command/target_precompile_headers.html
[gcc-pch]: https://gcc.gnu.org/onlinedocs/gcc/Precompiled-Headers.html
