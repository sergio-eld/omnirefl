cmake_minimum_required(VERSION 3.18.2)
project(instrumentation_pch_research LANGUAGES CXX)

# Standalone package consumer; all translation units are existing project tests.
get_filename_component(OMNIREFL_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(PCH_PROFILE functional CACHE STRING "none, standard, gtest, functional")
set(WORKLOAD core CACHE STRING "core or json")
set(TEST_STANDARD 20 CACHE STRING "Consumer C++ standard")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WORKLOAD STREQUAL "json")
    find_package(omnirefl CONFIG REQUIRED COMPONENTS serialization)
    set(_sources to_record.cpp to_enum.cpp dispatch.cpp)
    set(_source_directory "${OMNIREFL_SOURCE}/extras/extensions/serialization/tests")
else()
    find_package(omnirefl CONFIG REQUIRED)
    set(_sources static_members_test.cpp function_metadata_test.cpp
        interface_test.cpp dependent_types_test.cpp binding_metadata_test.cpp)
    set(_source_directory "${OMNIREFL_SOURCE}/extras/tests/tool")
endif()

include(FetchContent)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.tar.gz
    URL_HASH SHA256=81964fe578e9bd7c94dfdb09c8e4d6e6759e19967e397dbea48d1c10e45d0df2)
FetchContent_MakeAvailable(googletest)
if(NOT TARGET mpark_variant)
    set(MPARK_VARIANT_INCLUDE_TESTS "" CACHE STRING "" FORCE)
    FetchContent_Declare(mpark_variant
        URL https://github.com/mpark/variant/archive/refs/tags/v1.4.0.tar.gz
        URL_HASH SHA256=8f6b28ab3640b5d76d5b6664dda7257a4405ce59179220431b8fd196c79b2ecb)
    FetchContent_MakeAvailable(mpark_variant)
endif()

list(TRANSFORM _sources PREPEND "${_source_directory}/")
add_executable(pch_research.test ${_sources})
set_target_properties(pch_research.test PROPERTIES
    OUTPUT_NAME pch_research_test
    CXX_STANDARD ${TEST_STANDARD}
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)
target_include_directories(pch_research.test SYSTEM PRIVATE
    "${googletest_SOURCE_DIR}/googletest/include")
target_link_libraries(pch_research.test PRIVATE GTest::gtest_main mpark_variant)
if(WORKLOAD STREQUAL "json")
    target_link_libraries(pch_research.test PRIVATE omni::serialization)
else()
    target_link_libraries(pch_research.test PRIVATE omni::refl)
endif()

if(PCH_PROFILE STREQUAL "standard")
    target_precompile_headers(pch_research.test PRIVATE
        <memory> <string> <tuple> <type_traits> <utility> <vector>)
elseif(PCH_PROFILE STREQUAL "gtest")
    target_precompile_headers(pch_research.test PRIVATE <gtest/gtest.h>)
elseif(PCH_PROFILE STREQUAL "functional")
    target_precompile_headers(pch_research.test PRIVATE
        <gtest/gtest.h> <omnirefl/functional.hpp>)
elseif(NOT PCH_PROFILE STREQUAL "none")
    message(FATAL_ERROR "Unknown PCH_PROFILE: ${PCH_PROFILE}")
endif()
omni_reflected_target(pch_research.test TIMINGS)
