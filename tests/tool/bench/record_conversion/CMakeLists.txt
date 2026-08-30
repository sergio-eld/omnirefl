# 3.18.2 is the current project floor for CMake APIs used by the package and
# reflected target integration.
cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(record_conversion_bench LANGUAGES CXX)

include(FetchContent)

if(NOT TARGET omni::tool)
    find_package(omnirefl CONFIG REQUIRED)
endif()

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

option(RECORD_CONVERSION_ENABLE_PERF_COUNTERS
    "Enable Google Benchmark hardware counters through libpfm" OFF)
if(RECORD_CONVERSION_ENABLE_PERF_COUNTERS)
    set(BENCHMARK_ENABLE_LIBPFM ON CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(google_benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.5
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE)
FetchContent_MakeAvailable(google_benchmark)

add_executable(benchmark.record_conversion record_conversion.cpp)

set(_record_conversion_cpp_standard 20)
if("cxx_std_23" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    set(_record_conversion_cpp_standard 23)
endif()

set_target_properties(benchmark.record_conversion PROPERTIES
    CXX_STANDARD ${_record_conversion_cpp_standard}
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    OUTPUT_NAME record_conversion_bench)

target_link_libraries(benchmark.record_conversion PRIVATE benchmark::benchmark)

if(RECORD_CONVERSION_ENABLE_PERF_COUNTERS)
    target_compile_definitions(benchmark.record_conversion PRIVATE
        RECORD_CONVERSION_HAS_PERF_COUNTERS)
endif()

option(RECORD_CONVERSION_ENABLE_INDEX_MODE
    "Enable omnirefl index mode for the record conversion benchmark" ON)

set(_reflection_options LOG_LEVEL silent)
if(RECORD_CONVERSION_ENABLE_INDEX_MODE)
    list(APPEND _reflection_options ENABLE_INDEX_MODE)
endif()

omni_reflected_target(benchmark.record_conversion ${_reflection_options})
