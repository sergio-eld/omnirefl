#!/bin/sh
set -eu

readonly source=${1:-/workspaces/omnirefl}
readonly packages=${2:-/workspaces/omnirefl/artifacts/devcontainer}
readonly scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT

# Development builds only x86_64 unless a universal package is requested.
export COSMO_BUILD_UNIVERSAL=${COSMO_BUILD_UNIVERSAL:-false}

cd "$source"
if [ true = "$COSMO_BUILD_UNIVERSAL" ]; then
  cmake --preset cosmo -DOMNIREFL_PACKAGE_TARGET=cosmo-universal
else
  cmake --preset cosmo
fi
# Packaging needs the host tools, not the development tests and benchmarks.
cmake --build --preset cosmo --target tool ccdb_query --parallel

if [ true = "$COSMO_BUILD_UNIVERSAL" ]; then
  cmake --preset cosmo-aarch64
  cmake --build --preset cosmo-aarch64 --target tool ccdb_query --parallel
fi

bash docker/package-cosmo.sh "$source/build" "$scratch"
mkdir -p "$packages"
mv "$scratch"/*.tar.gz "$packages/omnirefl.tar.gz"
