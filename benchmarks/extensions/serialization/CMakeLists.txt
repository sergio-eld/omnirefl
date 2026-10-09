cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)
project(serialization_bench LANGUAGES CXX)
if(NOT TARGET omni::serialization)
    find_package(omnirefl CONFIG REQUIRED COMPONENTS serialization)
endif()
include(FetchContent)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(google_benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.5
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(google_benchmark)
include("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/toolchain_checks.cmake")
_omnirefl_supported_cpp_standards(_supported_cpp_standards)

foreach(_standard IN LISTS _supported_cpp_standards)
    set(_target "cpp_${_standard}.benchmark.serialization")
    add_executable(${_target} benchmark.cpp)
    set_target_properties(${_target} PROPERTIES
        CXX_STANDARD ${_standard} CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    # Reuse the serialization test models instead of duplicating them.
    target_include_directories(${_target} PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/../../../tests/extensions/serialization")
    if("GNU" STREQUAL CMAKE_CXX_COMPILER_FRONTEND_VARIANT)
        target_compile_options(${_target} PRIVATE -fno-exceptions)
    endif()
    target_link_libraries(${_target} PRIVATE
        benchmark::benchmark_main omni::serialization)
    omni_reflected_target(${_target} LOG_LEVEL silent)
endforeach()
