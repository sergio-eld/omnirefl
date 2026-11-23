cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_host_libstdcxx LANGUAGES CXX)

# Reproduce a host-libstdc++ type dependency that bundled libc++ cannot parse.
# The parent selects this fixture for native 64-bit GNU/Linux GCC builds.
find_package(omnirefl CONFIG REQUIRED)

add_executable(host_libstdcxx main.cpp)
set_target_properties(host_libstdcxx PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)
omni_reflected_target(host_libstdcxx LOG_LEVEL warning)
