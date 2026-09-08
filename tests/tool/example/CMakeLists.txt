# 3.18.2 is the current project floor for CMake APIs used by the package and
# reflected target integration.
cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(example LANGUAGES CXX)

if(NOT TARGET omni::tool)
    find_package(omnirefl CONFIG REQUIRED)
endif()

if(NOT "cxx_std_23" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    message(STATUS
        "omnirefl: skipping example: C++23 is not available")
    return()
endif()

# CMake's C++23 feature only checks the compiler flag. Compile and link the
# library facilities used by the example; MinGW may compile `println` but fail
# to link its terminal helpers.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/check_cxx23_library.cpp" [[
#include <format>
#include <print>

int main() {
  std::println("{}", std::format("{}", 815));
}
]])

try_compile(_has_cxx23_library
    "${CMAKE_CURRENT_BINARY_DIR}/check_cxx23_library"
    "${CMAKE_CURRENT_BINARY_DIR}/check_cxx23_library.cpp"
    CMAKE_FLAGS
        "-DCMAKE_CXX_STANDARD=23"
        "-DCMAKE_CXX_STANDARD_REQUIRED=ON"
        "-DCMAKE_CXX_EXTENSIONS=OFF"
    OUTPUT_VARIABLE _cxx23_library_check_output)

if(NOT _has_cxx23_library)
    message(STATUS
        "omnirefl: skipping example: required C++23 library facilities are not available")
    message(VERBOSE "${_cxx23_library_check_output}")
    return()
endif()

add_executable(example main.cpp)
set_property(TARGET example PROPERTY CXX_STANDARD 23)

# Reflection is not transitive: only this target's own .cpp files are
# instrumented. Call omni_reflected_target for each target that should be
# reflected.
omni_reflected_target(example)
