# 3.18.2 is the current project floor for CMake APIs used by the package.
cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_extension_examples LANGUAGES CXX)

# The main build provides its targets before adding this project. The copy
# installed under share/ instead discovers them from the installed package.
if(NOT _in_omnirefl_build)
    find_package(omnirefl CONFIG REQUIRED)
endif()

# TODO: Add per-extension examples and sandboxes as implementations mature.

# Extension dependencies are fetched by this dev project so source-tree and
# installed-package validation use the same dependency setup.
include(FetchContent)

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(INSTALL_GMOCK OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    # The newest GoogleTest release supporting C++11, kept in sync with the
    # main test project.
    GIT_TAG release-1.12.1)

set(EXPECTED_BUILD_PACKAGE OFF CACHE BOOL "" FORCE)
set(EXPECTED_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(tl_expected
    GIT_REPOSITORY https://github.com/TartanLlama/expected.git
    GIT_TAG v1.3.1
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE)

set(MPARK_VARIANT_INCLUDE_TESTS "" CACHE STRING "" FORCE)
FetchContent_Declare(mpark_variant
    GIT_REPOSITORY https://github.com/mpark/variant.git
    GIT_TAG v1.4.0
    GIT_SHALLOW TRUE
    GIT_SUBMODULES ""
    GIT_SUBMODULES_RECURSE FALSE)

set(RYML_INSTALL OFF CACHE BOOL "" FORCE)
# Keep rapidyaml's public compile feature from upgrading every versioned
# consumer to the parent Omnirefl project's C++23 default.
set(RYML_CXX_STANDARD 11 CACHE STRING "" FORCE)
FetchContent_Declare(rapidyaml
    GIT_REPOSITORY https://github.com/biojppm/rapidyaml.git
    # Preserve the dependency revision used by the recovered implementation.
    GIT_TAG 837255cfc4c62e1abf8b19c6f6068cd9246f125f
    GIT_PROGRESS TRUE)

if(ENABLE_BENCH)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(google_benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.9.5
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE)
endif()

# Dependency targets must retain the extension's C++11 floor instead of
# inheriting the parent Omnirefl tool's C++23 implementation standard.
set(_parent_cxx_standard "${CMAKE_CXX_STANDARD}")
set(CMAKE_CXX_STANDARD 11)
FetchContent_MakeAvailable(
    googletest
    tl_expected
    mpark_variant
    rapidyaml)
if(ENABLE_BENCH)
    FetchContent_MakeAvailable(google_benchmark)
endif()
if(_parent_cxx_standard)
    set(CMAKE_CXX_STANDARD "${_parent_cxx_standard}")
else()
    unset(CMAKE_CXX_STANDARD)
endif()

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    enable_testing()
endif()

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../tests/tool/supported_cpp_standards.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/../tests/tool/supported_cpp_standards.cmake")
else()
    include("${CMAKE_CURRENT_LIST_DIR}/../../tests/supported_cpp_standards.cmake")
endif()
_omnirefl_supported_cpp_standards(_supported_cpp_standards)

foreach(_standard IN LISTS _supported_cpp_standards)
    set(_test_target "cpp_${_standard}.sb.serialization")
    add_executable(${_test_target} serialization_test.cpp)
    set_target_properties(${_test_target} PROPERTIES
        CXX_STANDARD ${_standard}
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)
    target_link_libraries(${_test_target} PRIVATE
        gtest_main
        mpark_variant
        omni::refl
        ryml::ryml
        tl::expected)
    omni_reflected_target(${_test_target})
    add_test(NAME ${_test_target} COMMAND ${_test_target})

    if(ENABLE_BENCH AND 17 LESS_EQUAL _standard AND _standard LESS_EQUAL 23)
        set(_benchmark_target "cpp_${_standard}.benchmark.serialization")
        add_executable(${_benchmark_target}
            serialization_if_constexpr_bench.cpp
            serialization_composed_bench.cpp)
        set_target_properties(${_benchmark_target} PROPERTIES
            CXX_STANDARD ${_standard}
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO)
        target_link_libraries(${_benchmark_target} PRIVATE
            benchmark::benchmark_main
            mpark_variant
            omni::refl
            ryml::ryml
            tl::expected)
        omni_reflected_target(${_benchmark_target} LOG_LEVEL silent)
    endif()
endforeach()
