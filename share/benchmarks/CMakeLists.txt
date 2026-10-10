cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)
project(omnirefl_benchmarks LANGUAGES CXX)
enable_testing()
get_filename_component(omnirefl_PROJECTS_DIR
    "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT TARGET omni::tool)
    find_package(omnirefl CONFIG REQUIRED)
endif()
include("${omnirefl_PROJECTS_DIR}/cmake/test_dependencies.cmake")
include("${omnirefl_PROJECTS_DIR}/cmake/toolchain_checks.cmake")
omnirefl_supported_cpp_standards(_supported_cpp_standards)

option(RECORD_CONVERSION_ENABLE_PERF_COUNTERS
    "Enable Google Benchmark hardware counters through libpfm" OFF)
if(RECORD_CONVERSION_ENABLE_PERF_COUNTERS)
    set(BENCHMARK_ENABLE_LIBPFM ON CACHE BOOL "" FORCE)
endif()

# These settings apply to fetched Google Benchmark: omit its own tests and
# installation rules from Omnirefl's benchmark build and package.
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(google_benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.5
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE)
FetchContent_MakeAvailable(google_benchmark)

add_subdirectory(instrumentation)
omnirefl_check_cxx20_language(_benchmarks_has_cxx20)
if(_benchmarks_has_cxx20)
    add_subdirectory(record_conversion)
else()
    message(STATUS "omnirefl: skipping record_conversion: C++20 is not available")
endif()
if(NOT DEFINED BUILD_EXTENSIONS OR BUILD_EXTENSIONS)
    if(NOT TARGET omni::serialization)
        find_package(omnirefl CONFIG REQUIRED COMPONENTS serialization)
    endif()
    add_subdirectory("${omnirefl_PROJECTS_DIR}/extensions/serialization/benchmarks"
        extensions/serialization)
endif()
