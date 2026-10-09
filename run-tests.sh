#!/usr/bin/env bash
set -euo pipefail

UBUNTU_VERSIONS=(18.04 20.04 22.04)
TOOLCHAINS=(gcc clang mingw)

# Args: UBUNTU_VERSIONS... -- TOOLCHAINS...
if (($#)); then
  UBUNTU_VERSIONS=()
  TOOLCHAINS=()
  mode=vers
  for a in "$@"; do
    [[ "$a" == "--" ]] && { mode=comp; continue; }
    [[ $mode == vers ]] && UBUNTU_VERSIONS+=("$a") || TOOLCHAINS+=("$a")
  done
fi

PACKAGE_DIR=./artifacts/packages/current/musl docker compose run --rm build-musl
PACKAGE_DIR=./artifacts/packages/current/cosmo docker compose run --rm build-cosmo

readonly machine=$(uname -m)

for runtime in musl cosmo; do
  for toolchain in "${TOOLCHAINS[@]}"; do
    name="alpine-$machine-$runtime-$toolchain"
    PACKAGE_DIR="./artifacts/packages/current/$runtime" \
    TEST_NAME="$name" \
    TEST_RESULTS_DIR="./artifacts/test-results/$name" \
    TOOLCHAIN="$toolchain" docker compose run --rm test-alpine
  done

  for version in "${UBUNTU_VERSIONS[@]}"; do
    for toolchain in "${TOOLCHAINS[@]}"; do
      name="ubuntu-$version-$machine-$runtime-$toolchain"
      PACKAGE_DIR="./artifacts/packages/current/$runtime" \
      TEST_NAME="$name" \
      TEST_RESULTS_DIR="./artifacts/test-results/$name" \
      UBUNTU_VERSION="$version" TOOLCHAIN="$toolchain" \
        docker compose run --rm test-ubuntu
    done
  done
done
