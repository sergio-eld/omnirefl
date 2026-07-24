#!/usr/bin/env bash

set -euo pipefail

readonly build_dir=${1:-/build}
readonly source_dir=${2:-/src}
readonly package_dir=${3:-/packages}
readonly scratch_dir=$(mktemp -d /tmp/omnirefl-cosmopolitan-package.XXXXXX)
readonly binaries=(omnirefl ccdb_query)

restore_binaries() {
  for binary in "${binaries[@]}"; do
    cp "${scratch_dir}/${binary}" "${build_dir}/${binary}"
  done

  rm -r "${scratch_dir}"
}

trap restore_binaries EXIT

mkdir -p "${package_dir}"

for binary in "${binaries[@]}"; do
  cp "${build_dir}/${binary}" "${scratch_dir}/${binary}"
  test -f "${build_dir}/${binary}.com.dbg"
  test -f "${build_dir}/${binary}.aarch64.elf"
done

cmake -S "${source_dir}" -B "${build_dir}" \
  -DOMNIREFL_PACKAGE_TARGET=cosmopolitan-universal
cpack --config "${build_dir}/CPackConfig.cmake" -G TGZ -B "${scratch_dir}/cpack"

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
