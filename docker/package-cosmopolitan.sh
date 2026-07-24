#!/usr/bin/env bash

set -euo pipefail

readonly build_dir=${1:-/build}
readonly source_dir=${2:-/src}
readonly package_dir=${3:-/packages}
readonly cosmopolitan_root=${COSMOPOLITAN_ROOT:-/opt/cosmocc}
readonly scratch_dir=$(mktemp -d /tmp/omnirefl-cosmopolitan-package.XXXXXX)
readonly binaries=(omnirefl ccdb_query)
readonly aarch64_stub="${scratch_dir}/aarch64-package-stub"

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

printf '%s\n' \
  '#include <stdio.h>' \
  '' \
  'int main(void) {' \
  '  fputs("This omnirefl package requires macOS arm64.\n", stderr);' \
  '  return 126;' \
  '}' \
  > "${aarch64_stub}.c"
"${cosmopolitan_root}/bin/cosmocc" \
  -o "${aarch64_stub}" \
  "${aarch64_stub}.c"

for binary in "${binaries[@]}"; do
  cp "${scratch_dir}/${binary}" "${build_dir}/${binary}"
  "${cosmopolitan_root}/bin/assimilate" -cmx "${build_dir}/${binary}"
done

cmake -S "${source_dir}" -B "${build_dir}" \
  -DOMNIREFL_PACKAGE_TARGET=macos-x86_64-cosmopolitan
cpack --config "${build_dir}/CPackConfig.cmake" -G TGZ -B "${scratch_dir}/x86_64"
cp "${scratch_dir}/x86_64/"*.tar.gz "${package_dir}/"

for binary in "${binaries[@]}"; do
  "${cosmopolitan_root}/bin/apelink" \
    -V -1 \
    -l "${cosmopolitan_root}/bin/ape-x86_64.elf" \
    -l "${cosmopolitan_root}/bin/ape-aarch64.elf" \
    -M "${cosmopolitan_root}/bin/ape-m1.c" \
    -o "${build_dir}/${binary}" \
    "${aarch64_stub}.com.dbg" \
    "${build_dir}/${binary}.aarch64.elf"
done

cmake -S "${source_dir}" -B "${build_dir}" \
  -DOMNIREFL_PACKAGE_TARGET=macos-aarch64-cosmopolitan
cpack --config "${build_dir}/CPackConfig.cmake" -G TGZ -B "${scratch_dir}/aarch64"

readonly aarch64_archive=("${scratch_dir}/aarch64/"*.tar.gz)
readonly aarch64_package_dir="${scratch_dir}/aarch64-package"
mkdir -p "${aarch64_package_dir}"
tar -xzf "${aarch64_archive[0]}" -C "${aarch64_package_dir}"

readonly aarch64_package_root=("${aarch64_package_dir}/"*)
for binary in "${binaries[@]}"; do
  mv \
    "${aarch64_package_root[0]}/bin/${binary}" \
    "${aarch64_package_root[0]}/bin/${binary}.ape"
  printf '%s\n' \
    '#!/bin/sh' \
    '' \
    'exec /bin/sh "$0.ape" "$@"' \
    > "${aarch64_package_root[0]}/bin/${binary}"
  chmod +x "${aarch64_package_root[0]}/bin/${binary}"
done

tar -czf "${package_dir}/$(basename "${aarch64_archive[0]}")" \
  -C "${aarch64_package_dir}" \
  "$(basename "${aarch64_package_root[0]}")"
