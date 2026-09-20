cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_extension_tests LANGUAGES CXX)

if(NOT TARGET omni::refl)
    find_package(omnirefl CONFIG REQUIRED)
endif()

if(NOT TARGET omni::serialization)
    if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../extensions/serialization/dependencies.cmake")
        include("${CMAKE_CURRENT_LIST_DIR}/../../extensions/serialization/dependencies.cmake")
    else()
        include(serialization/dependencies.cmake)
    endif()
    find_package(omnirefl CONFIG REQUIRED COMPONENTS serialization)
endif()

include(FetchContent)
set(_extension_parent_standard "${CMAKE_CXX_STANDARD}")
set(CMAKE_CXX_STANDARD 11)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(INSTALL_GMOCK OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1)
FetchContent_MakeAvailable(googletest)

if(ENABLE_BENCH)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(google_benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.9.5
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE)
    FetchContent_MakeAvailable(google_benchmark)
endif()
if(_extension_parent_standard)
    set(CMAKE_CXX_STANDARD "${_extension_parent_standard}")
else()
    unset(CMAKE_CXX_STANDARD)
endif()

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    enable_testing()
endif()

if(NOT COMMAND _omnirefl_supported_cpp_standards)
    if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../tool/supported_cpp_standards.cmake")
        include("${CMAKE_CURRENT_LIST_DIR}/../tool/supported_cpp_standards.cmake")
    else()
        include("${CMAKE_CURRENT_LIST_DIR}/../supported_cpp_standards.cmake")
    endif()
endif()
_omnirefl_supported_cpp_standards(_supported_cpp_standards)

add_subdirectory(serialization)
