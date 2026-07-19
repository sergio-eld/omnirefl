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

docker compose run --rm build-linux

for toolchain in "${TOOLCHAINS[@]}"; do
  TOOLCHAIN="$toolchain" docker compose run --rm test-alpine
done

for version in "${UBUNTU_VERSIONS[@]}"; do
  for toolchain in "${TOOLCHAINS[@]}"; do
    UBUNTU_VERSION="$version" TOOLCHAIN="$toolchain" \
      docker compose run --rm test-ubuntu
  done
done
