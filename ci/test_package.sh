#!/usr/bin/env bash

set -euo pipefail

package=
name=
results=
cmake_arg=

while [ "$#" -gt 0 ]; do
  case $1 in
    --package)
      package=$2
      shift 2
      ;;
    --name)
      name=$2
      shift 2
      ;;
    --results)
      results=$2
      shift 2
      ;;
    --cmake-arg)
      if [ -n "$cmake_arg" ]; then
        printf 'only one cmake argument is supported\n' >&2
        exit 2
      fi
      cmake_arg=$2
      shift 2
      ;;
    *)
      printf 'unknown argument: %s\n' "$1" >&2
      exit 2
      ;;
  esac
done

if [ -z "$package" ]; then
  printf 'package is required\n' >&2
  exit 2
fi
if [ -z "$name" ]; then
  printf 'name is required\n' >&2
  exit 2
fi
if [ -z "$results" ]; then
  printf 'results is required\n' >&2
  exit 2
fi
if [ ! -f "$package" ]; then
  printf 'package does not exist: %s\n' "$package" >&2
  exit 2
fi

readonly work=$(mktemp -d "${TMPDIR:-/tmp}/omnirefl-package-test.XXXXXX")
readonly source="$work/with space/tests"
readonly build="$work/with space/build"
deb_installed=false

cleanup() {
  diagnostics="$results/diagnostics"
  mkdir -p "$diagnostics"
  for file in \
    "$build/CMakeCache.txt" \
    "$build/compile_commands.json" \
    "$build/Testing/Temporary/LastTest.log" \
    "$build/CMakeFiles/CMakeError.log"; do
    if [ -f "$file" ]; then
      cp "$file" "$diagnostics/"
    fi
  done

  (
    cd "$build"
    find . -type f \
      \( -name '*.omnirefl.hpp' -o -name '*.omnirefl.hpp.d' \) |
      tar -czf "$diagnostics/generated-reflection.tar.gz" -T - 2>/dev/null
  ) || true

  if $deb_installed; then
    apt remove -y omnirefl >/dev/null || true
  fi

  chmod -R a+rwX "$results" 2>/dev/null || true
  rm -rf "$work"
}

trap cleanup EXIT

mkdir -p "$results" "$source" "$build"

readonly package_name=$(basename "$package")

case "$package_name" in
  *-musl-*) readonly package_runtime=musl ;;
  *-cosmo-*) readonly package_runtime=cosmo ;;
  *)
    printf 'cannot infer package runtime from: %s\n' "$package_name" >&2
    exit 2
    ;;
esac

case "$package_name" in
  *.tar.gz)
    archive="$work/archive"
    mkdir -p "$archive"
    tar -xzf "$package" -C "$archive"

    set -- "$archive"/omnirefl-*
    if [ "$#" -ne 1 ] || [ ! -d "$1" ]; then
      printf 'expected one omnirefl package root in %s\n' "$package" >&2
      exit 1
    fi

    install="$work/with space/omnirefl"
    mv "$1" "$install"
    ;;
  *.deb)
    apt install -y "$package"
    deb_installed=true
    install=/usr
    ;;
  *)
    printf 'unsupported package: %s\n' "$package_name" >&2
    exit 2
    ;;
esac

readonly install
readonly config="$install/lib/cmake/omnirefl"
readonly packaged_tests="$install/share/omnirefl/tests"
readonly parallel=$(getconf _NPROCESSORS_ONLN 2>/dev/null \
  || sysctl -n hw.logicalcpu 2>/dev/null \
  || printf '1\n')

test -x "$install/bin/omnirefl"
test -x "$install/bin/ccdb_query"
test -d "$install/include/omnirefl"
test -d "$config"
test -d "$packaged_tests"

"$install/bin/omnirefl" --help 2>&1 |
  tee "$results/omnirefl-help.log"
"$install/bin/ccdb_query" --help 2>&1 |
  tee "$results/ccdb-query-help.log"

# A Cosmo archive keeps the APE payload beside its Unix launcher.
if [ "$package_runtime" = cosmo ]; then
  test -f "$install/bin/omnirefl.exe"
  test -f "$install/bin/ccdb_query.exe"
  /bin/sh "$install/bin/omnirefl.exe" --help 2>&1 |
    tee "$results/omnirefl-payload-help.log"
  /bin/sh "$install/bin/ccdb_query.exe" --help 2>&1 |
    tee "$results/ccdb-query-payload-help.log"
fi

cp -R "$packaged_tests"/. "$source/"

set -- cmake \
  -S "$source" \
  -B "$build" \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_BENCH=OFF \
  -Domnirefl_DIR="$config"
if [ -n "$cmake_arg" ]; then
  set -- "$@" "$cmake_arg"
fi
"$@" 2>&1 |
  tee "$results/configure.log"

cmake --build "$build" --parallel "$parallel" 2>&1 |
  tee "$results/build.log"

set +e
ctest --test-dir "$build" \
  --output-junit "$results/$name.xml" \
  -V 2>&1 |
  tee "$results/$name.log"
rc=$?
set -e

if [ "$rc" -ne 0 ] && [ -n "${KEEP_ALIVE_ON_FAILURE:-}" ]; then
  printf 'ctest failed with rc=%d; keeping container alive\n' "$rc"
  tail -f /dev/null
fi

exit "$rc"
