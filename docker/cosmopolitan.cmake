set(COSMOPOLITAN_ROOT "/opt/cosmocc" CACHE PATH "Root of the extracted cosmocc toolchain")
set(COSMOPOLITAN_ARCH "" CACHE STRING "Target Cosmopolitan architecture")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES COSMOPOLITAN_ARCH)

if(NOT COSMOPOLITAN_ARCH STREQUAL "x86_64"
    AND NOT COSMOPOLITAN_ARCH STREQUAL "aarch64")
    message(FATAL_ERROR
        "Unsupported Cosmopolitan architecture: ${COSMOPOLITAN_ARCH}")
endif()

# LLVM's platform configuration expects a supported CMake system name. The
# compilers still emit multi-OS executables for macOS and other hosts.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR "${COSMOPOLITAN_ARCH}")

set(_cosmopolitan_prefix
    "${COSMOPOLITAN_ROOT}/bin/${COSMOPOLITAN_ARCH}-unknown-cosmo")
set(CMAKE_C_COMPILER "${_cosmopolitan_prefix}-cc")
set(CMAKE_CXX_COMPILER "${_cosmopolitan_prefix}-c++")
set(CMAKE_AR "${_cosmopolitan_prefix}-ar")
set(CMAKE_RANLIB
    "${COSMOPOLITAN_ROOT}/bin/${COSMOPOLITAN_ARCH}-linux-cosmo-ranlib")

if(COSMOPOLITAN_ARCH STREQUAL "x86_64")
    set(CMAKE_CROSSCOMPILING_EMULATOR
        "${COSMOPOLITAN_ROOT}/bin/ape-x86_64.elf")
endif()
