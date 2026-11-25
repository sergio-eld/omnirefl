# Performance tracking

Use [`extras/benchmarks/compare.py`](../extras/benchmarks/compare.py) for local
A/B measurements of library changes. It is not run or compiled in CI.

Run it inside a Linux compiler container with Python 3.6+, Git, CMake 3.18.2+
and Ninja. Mount the repository and an installed Omnirefl package into that
container. For linked worktrees, mount their Git common directory too, at its
original absolute path. The package supplies the fixed instrumentation binary;
the compared sources supply the library headers.

```sh
python3 extras/benchmarks/compare.py \
  --baseline v0.0.8 --current WORKTREE \
  --package /opt/omnirefl --compiler c++ --std 20 \
  --output artifacts/performance/gcc-20 --repetitions 3
```

`--baseline` and `--current` accept Git refs or source directories. Refs and
`WORKTREE` are copied without changing the checkout. `WORKTREE` includes tracked
and untracked files except ignored files. Directory inputs are used directly;
keep them unchanged while measuring.

The default workload compiles public JSON reads and writes for flat records
with 20 and 160 integral fields. Add `--targets flat_80_read flat_160_read` to
select particular targets. `--flags` applies identical compiler options to both
sides. Wide tuples with some compiler/library combinations need higher template
depth or process stack limits; apply those equally and record the chosen flags.
A failed baseline stops the comparison. `--build-type Debug` measures
assert-enabled code too.

Each side uses the same workload, compiler, standard, flags and dependencies.
A warm-up precedes the measured repetitions; A/B order alternates. One compiler
process runs at a time so peak memory remains attributable to that target.
Only object outputs are removed between samples; generated metadata remains.
The compiler launcher measures backend wall time and peak resident memory,
excluding reflection, linking and configuration. Object-file bytes measure
object size, not final executable size or runtime performance.

The output contains `environment.json`, raw invocation records under `raw/`,
configuration/build logs and `summary.md`. The summary reports medians and the
coefficient of variation (CV) of elapsed time. Check CV and individual samples
before treating a small difference as an improvement. Reuse the same container
image and avoid competing builds when comparing runs.

`--project` selects another fixed CMake workload with one translation unit per
measured target. Its targets must use `OMNI_PERF_SOURCE_DIR` for the compared
library sources; see the standalone
[`record_shape_compile_cost` project](../extras/benchmarks/record_shape_compile_cost/CMakeLists.txt).
Repeat `--cmake-arg=-DNAME=value` for dependency-source overrides or other shared
configuration. Tool-generator changes need a separate comparison with separately
built packages; this script deliberately fixes the tool binary.
