cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(json_serialization LANGUAGES CXX)

# The serialization extension supports C++11; preserve newer compiler defaults.
if(NOT DEFINED CMAKE_CXX_STANDARD
    AND CMAKE_CXX_STANDARD_COMPUTED_DEFAULT STREQUAL "98")
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(NOT TARGET omni::serialization)
    find_package(omnirefl CONFIG REQUIRED COMPONENTS serialization)
endif()

enable_testing()

if(NOT TARGET gtest_main)
    include(FetchContent)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(INSTALL_GMOCK OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_Declare(googletest
        # Last Google Test release supporting C++11.
        URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.tar.gz
        URL_HASH SHA256=81964fe578e9bd7c94dfdb09c8e4d6e6759e19967e397dbea48d1c10e45d0df2)
    FetchContent_MakeAvailable(googletest)
    set_property(DIRECTORY "${googletest_SOURCE_DIR}"
        PROPERTY EXCLUDE_FROM_ALL TRUE)
endif()

add_executable(json_serialization main.cpp)
target_link_libraries(json_serialization PRIVATE omni::serialization gtest_main)
omni_reflected_target(json_serialization)

if(CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME)
    add_test(NAME example.json_serialization COMMAND json_serialization)
endif()
