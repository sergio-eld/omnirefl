# 3.18.2 is the current project floor for CMake APIs used by the package and
# reflected target integration.
cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(comprehensive_guide LANGUAGES CXX)

if(NOT TARGET omni::tool)
    find_package(omnirefl CONFIG REQUIRED)
endif()

enable_testing()
include(FetchContent)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(INSTALL_GMOCK OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1)
set(MPARK_VARIANT_INCLUDE_TESTS "" CACHE STRING "" FORCE)
FetchContent_Declare(mpark_variant
    GIT_REPOSITORY https://github.com/mpark/variant.git
    GIT_TAG v1.4.0
    GIT_SHALLOW TRUE
    GIT_SUBMODULES "")
FetchContent_MakeAvailable(googletest mpark_variant)

add_executable(comprehensive_guide comprehensive_guide.cpp)
set_property(TARGET comprehensive_guide PROPERTY CXX_STANDARD 20)

target_link_libraries(comprehensive_guide PRIVATE
    gtest
    gtest_main
    mpark_variant)

# Index mode is experimental and not expected to be stable across compilers.
omni_reflected_target(comprehensive_guide ENABLE_INDEX_MODE)

add_test(NAME "comprehensive_guide.run_omni"
    COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" -t comprehensive_guide.omni)

add_test(NAME "comprehensive_guide.build"
    COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" -t comprehensive_guide --parallel)

set_tests_properties("comprehensive_guide.build" PROPERTIES
    DEPENDS "comprehensive_guide.run_omni")

if(CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME)
    add_test(NAME "comprehensive_guide" COMMAND comprehensive_guide)
    set_tests_properties("comprehensive_guide" PROPERTIES
        DEPENDS "comprehensive_guide.build")
endif()
