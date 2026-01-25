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

find_package(GTest QUIET)
if (TARGET GTest::GTest)
    message(STATUS "GTest: found via find_package(), using existing GTest::GTest target")
else()
    message(STATUS "GTest: not found via find_package(), fetching googletest with FetchContent")

    # do not let googletest install itself when run "cmake --install" / CPack
    set(INSTALL_GTEST OFF CACHE BOOL "Disable installation of googletest" FORCE)
    set(INSTALL_GMOCK OFF CACHE BOOL "Disable installation of googlemock" FORCE)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2)

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    add_library(GTest::GTest ALIAS gtest)
endif()

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

FetchContent_MakeAvailable(tl_expected tl_optional mpark_variant)

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

message(VERBOSE "omnirefl: supported C++ standards (from CMAKE_CXX_COMPILE_FEATURES): ${_supported_cpp_standards}")

foreach(cpp_standard IN LISTS _supported_cpp_standards)
    set(cpp_prefix "cpp_${cpp_standard}")

    set(_static_odr_tgt "${cpp_prefix}.test.common.static_odr")
    add_library(${_static_odr_tgt} STATIC common/test_static_odr.cpp)
    target_compile_features(${_static_odr_tgt} PRIVATE cxx_std_${cpp_standard})
    target_compile_definitions(${_static_odr_tgt} PRIVATE CXX_STANDARD=${cpp_standard})
    target_link_libraries(${_static_odr_tgt} PRIVATE GTest::GTest std_compat)
    omni_reflected_target(${_static_odr_tgt})

    set(_test_common_tgt "${cpp_prefix}.test.common")
    add_executable(${_test_common_tgt}
        common/test_main.cpp
        common/dependent_types_test.cpp
        common/interface_test.cpp
        # common/example_test.cpp #< fixme
    )
    target_compile_features(${_test_common_tgt} PRIVATE cxx_std_${cpp_standard})
    target_compile_definitions(${_test_common_tgt} PRIVATE CXX_STANDARD=${cpp_standard})
    target_link_libraries(${_test_common_tgt} PRIVATE GTest::GTest std_compat ${_static_odr_tgt})
    omni_reflected_target(${_test_common_tgt})

    # fixme: the tool need to optimized by tracking the dependencies. 
    # As of now, .build will rerun the .run_omni, so will the actual test. 
    # With the std version matrix it WASTES a lot of time to run all of the tests...
    add_test(NAME "${cpp_prefix}.test.common.run_omni"
        COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" -t "${_test_common_tgt}.omni")

    add_test(NAME "${cpp_prefix}.test.common.build"
        COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" -t "${_test_common_tgt}")

    set_tests_properties("${cpp_prefix}.test.common.build" PROPERTIES DEPENDS "${cpp_prefix}.test.common.run_omni")

    if(CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME)
        add_test(NAME "${cpp_prefix}.test.common" COMMAND ${_test_common_tgt})
        set_tests_properties("${cpp_prefix}.test.common" PROPERTIES DEPENDS "${cpp_prefix}.test.common.build")
    endif()
endforeach()

# fixme: enable 
# function(add_reflection_test TEST_SOURCE_DIR)
#     # ------------------------------------------------------------------
#     # Options
#     #   MODES [HEADER|SOURCE] – list of modes to build; defaults to both
#     # ------------------------------------------------------------------
#     set(oneValueArgs)
#     set(multiValueArgs MODES)
#     cmake_parse_arguments(ARG "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
# 
#     if(ARG_MODES)
#         set(_modes ${ARG_MODES})
#     else()
#         set(_modes SOURCE HEADER)
#     endif()
# 
#     # Verify test.cpp exists
#     if(NOT EXISTS "${TEST_SOURCE_DIR}/test.cpp")
#         message(FATAL_ERROR
#             "add_reflection_test(): ${TEST_SOURCE_DIR}/test.cpp not found")
#     endif()
# 
#     get_filename_component(_dir_name "${TEST_SOURCE_DIR}" NAME)
#     set(_base_name "test.${_dir_name}")
# 
#     foreach(_mode ${_modes})
#         string(TOLOWER "${_mode}" _mode)
#         set(_suffix ".${_mode}-mode")
#         set(_target     "${_base_name}${_suffix}")
# 
#         add_executable(${_target} "${TEST_SOURCE_DIR}/test.cpp")
# 
#         # todo: enum standards 11, 14, 17, 20...
#         set_target_properties(${_target} PROPERTIES CXX_STANDARD 14)
# 
#         if(_mode STREQUAL "header")
#             reflected_target(${_target} HEADER_MODE PRINT_DEBUG)
#         else()
#             reflected_target(${_target} PRINT_DEBUG)
#         endif()
# 
#         target_link_libraries(${_target} PRIVATE GTest::gtest)
# 
#         add_test(NAME ${_target}.run_omni
#                  COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" -t ${_target}.omni)
# 
#         add_test(NAME ${_target}.build
#                  COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" -t ${_target})
# 
#         add_test(NAME ${_target} COMMAND ${_target})
# 
#         set_tests_properties(${_target}.build PROPERTIES DEPENDS ${_target}.run_omni)
#         set_tests_properties(${_target}       PROPERTIES DEPENDS ${_target}.build)
#     endforeach()
# endfunction()

# fixme: both modes
#   - fix `reflected_target` function:
#     setting forced include for a source file used by other targets will cause a conflict
#   - nested struct serialization for header-mode
# add_reflection_test(${CMAKE_CURRENT_SOURCE_DIR}/simple MODES SOURCE HEADER)
# add_subdirectory(multitarget)
# add_reflection_test(${CMAKE_CURRENT_SOURCE_DIR}/inplace MODES HEADER)
# todo: add test for failure

