# 3.18.2 is the current project floor for CMake APIs used by the package and
# reflected target integration.
cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(comprehensive_guide LANGUAGES CXX)

if(NOT TARGET omni::tool)
    find_package(omnirefl CONFIG REQUIRED)
endif()

enable_testing()
include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
# The guide uses Google Test without Google Mock.
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(INSTALL_GMOCK OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.tar.gz
    URL_HASH SHA256=81964fe578e9bd7c94dfdb09c8e4d6e6759e19967e397dbea48d1c10e45d0df2)
set(MPARK_VARIANT_INCLUDE_TESTS "" CACHE STRING "" FORCE)
FetchContent_Declare(mpark_variant
    URL https://github.com/mpark/variant/archive/refs/tags/v1.4.0.tar.gz
    URL_HASH SHA256=8f6b28ab3640b5d76d5b6664dda7257a4405ce59179220431b8fd196c79b2ecb)
set(_dependencies_parent_folder "${CMAKE_FOLDER}")
set(CMAKE_FOLDER "Dependencies")
FetchContent_MakeAvailable(googletest mpark_variant)
# An empty parent must unset CMAKE_FOLDER, not create an empty IDE group.
set(CMAKE_FOLDER ${_dependencies_parent_folder})
foreach(_dependency googletest mpark_variant)
    set_property(DIRECTORY "${${_dependency}_SOURCE_DIR}"
        PROPERTY EXCLUDE_FROM_ALL TRUE)
endforeach()

add_executable(comprehensive_guide comprehensive_guide.cpp)
set_property(TARGET comprehensive_guide PROPERTY CXX_STANDARD 20)

target_link_libraries(comprehensive_guide PRIVATE
    gtest
    gtest_main
    mpark_variant)

# Index mode is experimental and not expected to be stable across compilers.
omni_reflected_target(comprehensive_guide)

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
