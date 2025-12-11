#!/usr/bin/env bash
set -euo pipefail

# Defaults
VERSIONS=(18 20 22)
COMPILERS=(gcc clang) #< fixme: mingw)

# Args: VERSIONS... -- COMPILERS...
if (($#)); then
  VERSIONS=()
  COMPILERS=()
  mode=vers
  for a in "$@"; do
    [[ "$a" == "--" ]] && { mode=comp; continue; }
    [[ $mode == vers ]] && VERSIONS+=("$a") || COMPILERS+=("$a")
  done
fi

export UBUNTU_VERSION=""
export COMPILER=""

for job in build-linux build-tests-alpine package-linux; do
  docker compose run --rm "$job" || exit 1
done

for v in "${VERSIONS[@]}"; do
  for c in "${COMPILERS[@]}"; do
    UBUNTU_VERSION="$v" COMPILER="$c" \
      docker compose run --rm test-ubuntu || exit 1
  done
done
