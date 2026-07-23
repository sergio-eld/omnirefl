set(COSMOPOLITAN_ROOT "/opt/cosmocc" CACHE PATH "Root of the extracted cosmocc toolchain")

# LLVM's platform configuration expects a supported CMake system name. The
# compilers still emit Actually Portable Executables for macOS and other hosts.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER "${COSMOPOLITAN_ROOT}/bin/cosmocc")
set(CMAKE_CXX_COMPILER "${COSMOPOLITAN_ROOT}/bin/cosmoc++")
set(CMAKE_AR "${COSMOPOLITAN_ROOT}/bin/cosmoar")
set(CMAKE_RANLIB "${COSMOPOLITAN_ROOT}/bin/cosmoranlib")
set(CMAKE_CROSSCOMPILING_EMULATOR "${COSMOPOLITAN_ROOT}/bin/ape-x86_64.elf")
