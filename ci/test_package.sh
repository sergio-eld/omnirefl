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

# ZIP extraction changes directory; retain an absolute archive path.
package=$(cd "$(dirname "$package")" && pwd)/$(basename "$package")

readonly work=$(mktemp -d "${TMPDIR:-/tmp}/omnirefl-package-test.XXXXXX")
readonly source="$work/with space"
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
  *-ucrt-*) readonly package_runtime=ucrt ;;
  *)
    printf 'cannot infer package runtime from: %s\n' "$package_name" >&2
    exit 2
    ;;
esac

# MSYS2 selects Windows executables rather than the Unix launchers.
case $(uname -s) in
  MINGW* | MSYS*) readonly executable_suffix=.exe ;;
  *) readonly executable_suffix= ;;
esac

case "$package_name" in
  *.tar.gz | *.zip)
    archive="$work/archive"
    mkdir -p "$archive"
    if [[ "$package_name" = *.zip ]]; then
      (cd "$archive" && cmake -E tar xf "$package")
    elif [ "$package_runtime" = cosmo ] && [ -n "$executable_suffix" ]; then
      # MSYS2 tar aliases .exe names with their Unix launchers. Windows tar
      # preserves both package files; remove this workaround when MSYS2 does.
      /c/Windows/System32/tar.exe -xzf "$package" -C "$archive"
    else
      tar -xzf "$package" -C "$archive"
    fi

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
readonly packaged_examples="$install/share/omnirefl/examples"
readonly packaged_tests="$install/share/omnirefl/tests"
readonly parallel=$(getconf _NPROCESSORS_ONLN 2>/dev/null \
  || sysctl -n hw.logicalcpu 2>/dev/null \
  || printf '1\n')

test -x "$install/bin/omnirefl$executable_suffix"
test -x "$install/bin/ccdb_query$executable_suffix"
test -d "$install/include/omnirefl"
test -d "$config"
test -d "$packaged_examples"
test -d "$packaged_tests"

for tool in omnirefl ccdb_query; do
  if [ "$package_runtime" = cosmo ] && [ -n "$executable_suffix" ]; then
    # MSYS2 cannot launch the Cosmo PE payload directly; PowerShell uses the
    # Windows loader. Remove this workaround when MSYS2 can launch it.
    OMNIREFL_TEST_EXECUTABLE="$(cygpath -w "$install/bin/$tool.exe")" \
      powershell.exe -NoProfile -Command \
        '& $env:OMNIREFL_TEST_EXECUTABLE --help; exit $LASTEXITCODE' 2>&1 |
      tee "$results/${tool/_/-}-help.log"
  else
    "$install/bin/$tool$executable_suffix" --help 2>&1 |
      tee "$results/${tool/_/-}-help.log"
  fi
done

# A Cosmo archive keeps the APE payload beside its Unix launcher.
if [ "$package_runtime" = cosmo ] && [ -z "$executable_suffix" ]; then
  test -f "$install/bin/omnirefl.exe"
  test -f "$install/bin/ccdb_query.exe"
  /bin/sh "$install/bin/omnirefl.exe" --help 2>&1 |
    tee "$results/omnirefl-payload-help.log"
  /bin/sh "$install/bin/ccdb_query.exe" --help 2>&1 |
    tee "$results/ccdb-query-payload-help.log"
fi

cp -R "$install/share/omnirefl"/. "$work/with space/"

set -- cmake \
  -S "$source" \
  -B "$build" \
  -DBUILD_BENCHMARKS=OFF \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -Domnirefl_ROOT="$install"
if [ -n "$cmake_arg" ]; then
  set -- "$@" "$cmake_arg"
fi
if ! "$@" 2>&1 | tee "$results/configure.log"; then
  printf 'omnirefl: packaged project configuration failed\n' >&2
  exit 1
fi

if ! cmake --build "$build" --parallel "$parallel" 2>&1 | tee "$results/build.log"; then
  printf 'omnirefl: packaged project build failed\n' >&2
  exit 1
fi

set +e
ctest --test-dir "$build" --no-tests=error \
  --timeout 600 \
  --output-junit "$results/$name.xml" \
  -V 2>&1 |
  tee "$results/$name.log"
rc=$?
set -e

if [ 0 -ne "$rc" ]; then
  printf 'omnirefl: packaged project tests failed\n' >&2
fi

if [ "$rc" -ne 0 ] && [ -n "${KEEP_ALIVE_ON_FAILURE:-}" ]; then
  printf 'ctest failed with rc=%d; keeping container alive\n' "$rc"
  tail -f /dev/null
fi

exit "$rc"
