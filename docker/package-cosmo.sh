#!/usr/bin/env bash

set -euo pipefail

readonly build_dir=${1:-/build}
readonly package_dir=${2:-/packages}
readonly cosmopolitan_root=${COSMOPOLITAN_ROOT:-/opt/cosmocc}
readonly scratch_dir=$(mktemp -d /tmp/omnirefl-cosmo-package.XXXXXX)
readonly binaries=(omnirefl ccdb_query)

architectures=("$(uname -m)")
if [ true = "${COSMO_BUILD_UNIVERSAL:-true}" ]; then
  architectures=(x86_64 aarch64)
fi
readonly -a architectures
# The first architecture owns the CPack configuration and package input files.
readonly primary_arch=${architectures[0]}

loaders=()
for arch in "${architectures[@]}"; do
  loaders+=(-l "${cosmopolitan_root}/bin/ape-${arch}.elf")
  # The macOS ARM loader source requires an AArch64 payload.
  if [ aarch64 = "${arch}" ]; then
    loaders+=(-M "${cosmopolitan_root}/bin/ape-m1.c")
  fi
done

# Ad hoc: CPack installs from the primary build tree, so apelink temporarily
# replaces its binaries with APE output. The EXIT trap restores the originals
# on success or failure and removes the packaging scratch directory.
# TODO: [low] Consider staging installation separately to leave build outputs intact.
restore_binaries() {
  for binary in "${binaries[@]}"; do
    cp "${scratch_dir}/${binary}" "${build_dir}/${primary_arch}/${binary}"
  done

  rm -r "${scratch_dir}"
}

for binary in "${binaries[@]}"; do
  cp "${build_dir}/${primary_arch}/${binary}" "${scratch_dir}/${binary}"
done
trap restore_binaries EXIT

mkdir -p "${package_dir}"

# apelink drops DWARF sections; retain each architecture's symbols beforehand.
for arch in "${architectures[@]}"; do
  mkdir -p "${build_dir}/${primary_arch}/symbols/${arch}"
  for binary in "${binaries[@]}"; do
    "${cosmopolitan_root}/bin/${arch}-unknown-cosmo-objcopy" \
      --only-keep-debug "${build_dir}/${arch}/${binary}" \
      "${build_dir}/${primary_arch}/symbols/${arch}/${binary}.debug"
  done
done

# Cosmopolitan supports building each architecture separately and combining
# their executables with apelink:
# https://github.com/jart/cosmopolitan/blob/master/tool/cosmocc/README.md#overview
for binary in "${binaries[@]}"; do
  # Use the primary backup as input because apelink replaces its build output.
  inputs=("${scratch_dir}/${binary}")
  for arch in "${architectures[@]:1}"; do
    inputs+=("${build_dir}/${arch}/${binary}")
  done
  "${cosmopolitan_root}/bin/apelink" \
    -V -1 \
    "${loaders[@]}" \
    -o "${build_dir}/${primary_arch}/${binary}" \
    "${inputs[@]}"
done

# Headers and other packaged resources are shared between architectures.
cpack \
  --config "${build_dir}/${primary_arch}/CPackConfig.cmake" \
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

# Native packages already export their host executable names. Only Cosmo has
# both Unix launchers and Windows executables in the same package.
for targets in "${package_root[0]}"/*/cmake/omnirefl/omnireflTargets-*.cmake; do
  {
    cat <<'EOF'
set(_omnirefl_executable_suffix "")
if(CMAKE_HOST_WIN32)
  set(_omnirefl_executable_suffix ".exe")
endif()
EOF
    # Adapt both imported locations and CMake's file-existence checks.
    sed 's|${_IMPORT_PREFIX}/bin/[^"]*|&${_omnirefl_executable_suffix}|g' \
      "${targets}"
    printf '\nunset(_omnirefl_executable_suffix)\n'
  } > "${scratch_dir}/targets.cmake"
  mv "${scratch_dir}/targets.cmake" "${targets}"
done

tar -czf "${package_dir}/$(basename "${archive[0]}")" \
  -C "${archive_dir}" \
  "$(basename "${package_root[0]}")"
