#!/usr/bin/env bash

set -euo pipefail

readonly x86_64_build_dir=${1:-/build/x86_64}
readonly aarch64_build_dir=${2:-/build/aarch64}
readonly package_dir=${3:-/packages}
readonly cosmopolitan_root=${COSMOPOLITAN_ROOT:-/opt/cosmocc}
readonly scratch_dir=$(mktemp -d /tmp/omnirefl-cosmo-package.XXXXXX)
readonly binaries=(omnirefl ccdb_query)

restore_binaries() {
  for binary in "${binaries[@]}"; do
    cp "${scratch_dir}/${binary}.x86_64" "${x86_64_build_dir}/${binary}"
  done

  rm -r "${scratch_dir}"
}

for binary in "${binaries[@]}"; do
  cp "${x86_64_build_dir}/${binary}" "${scratch_dir}/${binary}.x86_64"
done
trap restore_binaries EXIT

mkdir -p "${package_dir}"

# apelink drops DWARF sections; retain each architecture's symbols beforehand.
for arch in x86_64 aarch64; do
  build_dir=$x86_64_build_dir
  if [ aarch64 = "$arch" ]; then
    build_dir=$aarch64_build_dir
  fi
  mkdir -p "${x86_64_build_dir}/symbols/${arch}"
  for binary in "${binaries[@]}"; do
    "${cosmopolitan_root}/bin/${arch}-unknown-cosmo-objcopy" \
      --only-keep-debug "${build_dir}/${binary}" \
      "${x86_64_build_dir}/symbols/${arch}/${binary}.debug"
  done
done

# Cosmopolitan supports building each architecture separately and combining
# their executables with apelink:
# https://github.com/jart/cosmopolitan/blob/master/tool/cosmocc/README.md#overview
for binary in "${binaries[@]}"; do
  "${cosmopolitan_root}/bin/apelink" \
    -V -1 \
    -l "${cosmopolitan_root}/bin/ape-x86_64.elf" \
    -l "${cosmopolitan_root}/bin/ape-aarch64.elf" \
    -M "${cosmopolitan_root}/bin/ape-m1.c" \
    -o "${x86_64_build_dir}/${binary}" \
    "${scratch_dir}/${binary}.x86_64" \
    "${aarch64_build_dir}/${binary}"
done

# Ad hoc: CPack installs from the x86_64 build tree, so apelink temporarily
# replaces its binaries with universal ones; the EXIT trap restores them.
# Headers and other packaged resources are shared between architectures.
# TODO: [low] Consider staging installation separately to leave build outputs intact.
cpack \
  --config "${x86_64_build_dir}/CPackConfig.cmake" \
  -G TGZ \
  -B "${scratch_dir}/cpack"

readonly archive=("${scratch_dir}/cpack/"*.tar.gz)
readonly archive_dir="${scratch_dir}/archive"
mkdir -p "${archive_dir}"
tar -xzf "${archive[0]}" -C "${archive_dir}"

readonly package_root=("${archive_dir}/"*)
for binary in "${binaries[@]}"; do
  mv \
    "${package_root[0]}/bin/${binary}" \
    "${package_root[0]}/bin/${binary}.exe"
  printf '%s\n' \
    '#!/bin/sh' \
    '' \
    'exec /bin/sh "$0.exe" "$@"' \
    > "${package_root[0]}/bin/${binary}"
  chmod +x "${package_root[0]}/bin/${binary}"
done

tar -czf "${package_dir}/$(basename "${archive[0]}")" \
  -C "${archive_dir}" \
  "$(basename "${package_root[0]}")"
