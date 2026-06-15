cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_tests LANGUAGES CXX)

enable_testing()

if (NOT TARGET omni::tool)
    find_package(omnirefl CONFIG REQUIRED)
endif()

# -- Dependencies --------
include(FetchContent)

if(NOT TARGET omni::tool)
    find_package(omnirefl CONFIG REQUIRED)
endif()

# Enable FetchContent logging
set(FETCHCONTENT_QUIET OFF CACHE BOOL "" FORCE)

# do not let googletest install itself when run "cmake --install" / CPack
set(INSTALL_GTEST OFF CACHE BOOL "Disable installation of googletest" FORCE)
set(INSTALL_GMOCK OFF CACHE BOOL "Disable installation of googlemock" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
     # The "newest" gtest that supports C++11. 
     # Otherwize will force its C++ version upon the test targets.
    GIT_TAG release-1.12.1)

# std-compat utility
set(EXPECTED_BUILD_PACKAGE OFF CACHE BOOL "" FORCE)
set(EXPECTED_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(tl_expected
    GIT_REPOSITORY https://github.com/TartanLlama/expected.git
    GIT_TAG v1.3.1
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE)

set(OPTIONAL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(tl_optional
    GIT_REPOSITORY https://github.com/TartanLlama/optional.git
    GIT_TAG v1.1.0
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE)

set(MPARK_VARIANT_INCLUDE_TESTS "" CACHE STRING "" FORCE)
FetchContent_Declare(mpark_variant
    GIT_REPOSITORY https://github.com/mpark/variant.git
    GIT_TAG v1.4.0
    GIT_SHALLOW TRUE
    GIT_SUBMODULES ""
    GIT_SUBMODULES_RECURSE FALSE
)

FetchContent_MakeAvailable(tl_expected tl_optional mpark_variant googletest)
add_library(GTest::GTest ALIAS gtest)

# todo: move header with `compat::variant`, `compat::expected` and `compat::optional` here.
add_library(std_compat INTERFACE)
target_link_libraries(std_compat INTERFACE
    tl::expected
    tl::optional
    mpark_variant
)

# Enumerate supported C++ standard levels (>=11) and build targets prefixed with cpp_<N>.
set(_supported_cpp_standards "")
foreach(_f IN LISTS CMAKE_CXX_COMPILE_FEATURES)
    if(_f MATCHES "^cxx_std_([0-9]+)$")
        set(_n "${CMAKE_MATCH_1}")

        # Exclude legacy 98/03 (they are numerically >= 11), keep 11+ "year-style" standards.
        if(_n GREATER_EQUAL 11 AND _n LESS 90)
            list(APPEND _supported_cpp_standards "${_n}")
        endif()
    endif()
endforeach()

list(REMOVE_DUPLICATES _supported_cpp_standards)
list(SORT _supported_cpp_standards)

# MSVC has no distinct /std:c++11; CMake effectively treats it as C++14.
# Normalize 11 -> 14 to avoid generating duplicate cpp_11 / cpp_14 targets.
if(MSVC)
  set(_tmp "")
  foreach(_n IN LISTS _supported_cpp_standards)
    if(_n STREQUAL "11")
      set(_n "14")
    endif()
    list(APPEND _tmp "${_n}")
  endforeach()
  set(_supported_cpp_standards "${_tmp}")
  list(REMOVE_DUPLICATES _supported_cpp_standards)
  list(SORT _supported_cpp_standards)
endif()

message(VERBOSE "omnirefl: supported C++ standards (from CMAKE_CXX_COMPILE_FEATURES): ${_supported_cpp_standards}")

foreach(cpp_standard IN LISTS _supported_cpp_standards)
    set(cpp_prefix "cpp_${cpp_standard}")

    set(_static_odr_tgt "${cpp_prefix}.test.static_odr")
    add_library(${_static_odr_tgt} STATIC test_static_odr.cpp)
    set_target_properties(${_static_odr_tgt} PROPERTIES
        CXX_STANDARD ${cpp_standard}
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )

    target_compile_definitions(${_static_odr_tgt} PRIVATE CXX_STANDARD=${cpp_standard})
    target_link_libraries(${_static_odr_tgt} PRIVATE GTest::GTest std_compat)
    omni_reflected_target(${_static_odr_tgt} ENABLE_INDEX_MODE)

    set(_test_tgt "${cpp_prefix}.test")
    set(_test_sources
        test_main.cpp
        dependent_types_test.cpp
        interface_test.cpp
        example_test.cpp
        indexed_types.cpp
        inplace_test.cpp
    )

    add_executable(${_test_tgt} ${_test_sources})

    set_target_properties(${_test_tgt} PROPERTIES
        CXX_STANDARD ${cpp_standard}
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )

    target_compile_definitions(${_test_tgt} PRIVATE CXX_STANDARD=${cpp_standard})
    target_link_libraries(${_test_tgt} PRIVATE GTest::GTest std_compat ${_static_odr_tgt})
    omni_reflected_target(${_test_tgt} ENABLE_INDEX_MODE)

    add_test(NAME "${cpp_prefix}.test.run_omni"
        COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" -t "${_test_tgt}.omni")

    add_test(NAME "${cpp_prefix}.test.build"
        COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" -t "${_test_tgt}")

    set_tests_properties("${cpp_prefix}.test.build" PROPERTIES
        DEPENDS "${cpp_prefix}.test.run_omni")

    if(CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME)
        add_test(NAME "${cpp_prefix}.test" COMMAND ${_test_tgt})
        set_tests_properties("${cpp_prefix}.test" PROPERTIES
            DEPENDS "${cpp_prefix}.test.build")
    endif()
endforeach()
