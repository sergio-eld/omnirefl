# Benchmarks

Configure the benchmarks from the repository root:

```sh
cmake -S . -B build -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_TESTING=ON \
  -DENABLE_BENCH=ON
```

For example, build the record-conversion benchmark:

```sh
cmake --build build --target benchmark.record_conversion
```

Run the record-conversion benchmark:

```sh
./build/tests/bench/record_conversion/record_conversion_bench \
  --benchmark_min_time=0.1s \
  --benchmark_repetitions=7 \
  --benchmark_enable_random_interleaving=true
```

The compact report uses `aggregate_return` as the baseline for each layout and
the median when repetitions are enabled. `instructions/record` means CPU
instructions retired per converted record. Effective `GiB/s` counts the source
record read and destination record written.

## Hardware counters

Google Benchmark can collect hardware counters through `libpfm` on Linux. For
example, configure and build the record-conversion benchmark with counters:

```sh
cmake -S . -B build-perf -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_TESTING=ON \
  -DENABLE_BENCH=ON \
  -DRECORD_CONVERSION_ENABLE_PERF_COUNTERS=ON
cmake --build build-perf --target benchmark.record_conversion
```

This configuration collects `instructions` and `cycles` by default. Pass
`--benchmark_perf_counters=...` to select another counter set.

Counter names are CPU- and `libpfm`-specific. On the tested Intel CPU,
`icache_64b.iftag_hit` and `icache_64b.iftag_miss` exposed the instruction-cache
tag-lookup ratio; generic `L1-icache-loads` was unsupported.

Docker requires PMU access. On WSL2, the working minimum was:

```sh
docker run --cap-add PERFMON --security-opt seccomp=unconfined ...
```

The default Docker profile could not open `cycles`. Use native Linux for final
measurements.

## Assembly

The conversion batches are not inlined. Inspect them with:

```sh
objdump -dC --no-show-raw-insn \
  ./build/tests/bench/record_conversion/record_conversion_bench \
  | less
```

Search for `raw`, `ranges_transform`, and `ranges_to`.
