#!/usr/bin/env bash

set -euo pipefail

install=
build=
results=
name=

while (($#)); do
  case $1 in
    --install)
      install=$2
      shift 2
      ;;
    --build)
      build=$2
      shift 2
      ;;
    --results)
      results=$2
      shift 2
      ;;
    --name)
      name=$2
      shift 2
      ;;
    *)
      printf 'unknown argument: %s\n' "$1" >&2
      exit 2
      ;;
  esac
done

if [[ -z $install ]]; then
  printf 'install is required\n' >&2
  exit 2
fi
if [[ -z $build ]]; then
  printf 'build is required\n' >&2
  exit 2
fi
if [[ -z $results ]]; then
  printf 'results is required\n' >&2
  exit 2
fi
if [[ -z $name ]]; then
  printf 'name is required\n' >&2
  exit 2
fi

timestamp_ns() {
  if command -v python3 >/dev/null; then
    python3 -c 'import time; print(time.monotonic_ns())'
    return
  fi

  perl -MTime::HiRes=clock_gettime,CLOCK_MONOTONIC \
    -e 'printf "%.0f\n", clock_gettime(CLOCK_MONOTONIC) * 1_000_000_000'
}

readonly processors=$(getconf _NPROCESSORS_ONLN 2>/dev/null \
  || sysctl -n hw.logicalcpu 2>/dev/null \
  || printf '1\n')
if ((2 < processors)); then
  readonly parallel=$((processors - 2))
else
  readonly parallel=$processors
fi

mkdir -p "$build" "$results"

printf 'Configuring benchmark...\n'
cmake -S "$install/share/omnirefl/tests" \
  -B "$build" \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_BENCH=ON \
  -Domnirefl_DIR="$install/lib/cmake/omnirefl" 2>&1 |
  tee "$results/configure.log"

printf 'Generating reflection...\n'
reflection_start=$(timestamp_ns)
if ! cmake --build "$build" -t benchmark.baseline.omni \
    > "$results/reflection.log" 2>&1; then
  cat "$results/reflection.log"
  exit 1
fi
reflection_end=$(timestamp_ns)
reflection_ms=$(awk \
  -v start="$reflection_start" \
  -v end="$reflection_end" \
  'BEGIN { printf "%.3f", (end - start) / 1000000 }')

printf 'Building benchmark with %s parallel jobs...\n' "$parallel"
build_start=$(timestamp_ns)
if ! cmake --build "$build" \
    -t benchmark.baseline \
    --parallel "$parallel" \
    > "$results/build.log" 2>&1; then
  cat "$results/build.log"
  exit 1
fi
build_end=$(timestamp_ns)
build_ms=$(awk \
  -v start="$build_start" \
  -v end="$build_end" \
  'BEGIN { printf "%.3f", (end - start) / 1000000 }')

tool_build=$(awk -v tool="$reflection_ms" -v build="$build_ms" \
  'BEGIN { printf "%.6f", (0 == build ? 0 : 100 * tool / build) }')

{
  printf '[\n'
  printf '  {"name": "reflection wall", "unit": "ms", "value": %s},\n' \
    "$reflection_ms"
  printf '  {"name": "build wall", "unit": "ms", "value": %s},\n' \
    "$build_ms"
  printf '  {"name": "tool/build", "unit": "%%", "value": %s}' \
    "$tool_build"
  grep -E '^[^:]+: .+ \([0-9]+ microseconds\)$' "$results/reflection.log" |
    while IFS= read -r line; do
      metric=${line%%:*}
      us=$(printf '%s\n' "$line" \
        | sed -E 's/.*\(([0-9]+) microseconds\)$/\1/')
      ms=$(awk -v us="$us" 'BEGIN { printf "%.3f", us / 1000 }')
      printf ',\n  {"name": "tool %s", "unit": "ms", "value": %s}' \
        "$metric" "$ms"
    done
  printf '\n]\n'
} > "$results/benchmark.json"

printf '\nBenchmark summary (%s)\n' "$name"
printf '%-32s %12s %8s\n' 'metric' 'value' 'unit'
printf '%-32s %12s %8s\n' 'reflection wall' "$reflection_ms" 'ms'
printf '%-32s %12s %8s\n' 'build wall' "$build_ms" 'ms'
printf '%-32s %12s %8s\n' 'tool/build' "$tool_build" '%'
grep -E '^[^:]+: .+ \([0-9]+ microseconds\)$' "$results/reflection.log" |
  while IFS= read -r line; do
    metric=${line%%:*}
    us=$(printf '%s\n' "$line" \
      | sed -E 's/.*\(([0-9]+) microseconds\)$/\1/')
    ms=$(awk -v us="$us" 'BEGIN { printf "%.3f", us / 1000 }')
    printf '%-32s %12s %8s\n' "tool $metric" "$ms" 'ms'
  done
