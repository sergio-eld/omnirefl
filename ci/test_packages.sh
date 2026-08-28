#!/usr/bin/env bash

set -euo pipefail

packages=
name=
results=
cmake_arg=

while [ "$#" -gt 0 ]; do
  case $1 in
    --packages)
      packages=$2
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

if [ -z "$packages" ]; then
  printf 'packages is required\n' >&2
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

readonly machine=$(uname -m)
package_musl=
package_cosmo=

for package in "$packages"/*.tar.gz; do
  case $(basename "$package") in
    omnirefl-linux-"$machine"-musl-*)
      if [ -n "$package_musl" ]; then
        printf 'multiple musl packages for %s\n' "$machine" >&2
        exit 1
      fi
      package_musl=$package
      ;;
    omnirefl-cosmo-universal-*)
      if [ -n "$package_cosmo" ]; then
        printf 'multiple cosmo packages\n' >&2
        exit 1
      fi
      package_cosmo=$package
      ;;
  esac
done

if [ -z "$package_musl" ]; then
  printf 'musl package for %s was not found\n' "$machine" >&2
  exit 1
fi
if [ -z "$package_cosmo" ]; then
  printf 'cosmo package was not found\n' >&2
  exit 1
fi

for package in "$package_musl" "$package_cosmo"; do
  case $(basename "$package") in
    *-musl-*) runtime=musl ;;
    *-cosmo-*) runtime=cosmo ;;
  esac

  set -- "${PACKAGE_TEST_SHELL:-/bin/bash}" /runner/test_package.sh \
    --package "$package" \
    --name "$name-$runtime" \
    --results "$results/$runtime"
  if [ -n "$cmake_arg" ]; then
    set -- "$@" --cmake-arg "$cmake_arg"
  fi
  "$@"
done
