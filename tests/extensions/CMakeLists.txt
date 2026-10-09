cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_extension_tests LANGUAGES CXX)

if(NOT TARGET omni::refl)
    find_package(omnirefl CONFIG REQUIRED)
endif()

if(NOT TARGET omni::serialization)
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

if(_extension_parent_standard)
    set(CMAKE_CXX_STANDARD "${_extension_parent_standard}")
else()
    unset(CMAKE_CXX_STANDARD)
endif()

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    enable_testing()
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/toolchain_checks.cmake")
_omnirefl_supported_cpp_standards(_supported_cpp_standards)

add_subdirectory(serialization)
