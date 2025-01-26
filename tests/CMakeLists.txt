cmake_minimum_required(VERSION 3.14.2 FATAL_ERROR)

project(omnirefl_tests LANGUAGES CXX)
# todo: this file/subfolder should be packaged for testing on different platforms

if (NOT TARGET omni::tool)
    find_package(omnirefl CONFIG REQUIRED)
endif()

find_package(GTest QUIET)
if (NOT TARGET GTest::GTest)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    add_library(GTest::GTest ALIAS gtest)
endif()
enable_testing()

add_subdirectory(simple)
add_subdirectory(odr)
# todo: add test for failure

