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

mkdir -p "$build" "$results"
cmake -S "$install/share/omnirefl" \
  -B "$build" -GNinja -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF \
  -Domnirefl_ROOT="$install" \
  -DBENCHMARK_RESULTS="$results" -DBENCHMARK_NAME="$name" 2>&1 |
  tee "$results/configure.log"
ctest --test-dir "$build" -R '^benchmark\.instrumentation$' \
  --no-tests=error --output-on-failure
