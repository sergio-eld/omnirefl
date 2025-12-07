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

# Do not let googletest install itself when we run "cmake --install" / CPack
set(INSTALL_GTEST OFF CACHE BOOL "Disable installation of googletest" FORCE)
set(INSTALL_GMOCK OFF CACHE BOOL "Disable installation of googlemock" FORCE)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
add_library(GTest::GTest ALIAS gtest)

# std-compat utility
set(EXPECTED_BUILD_PACKAGE OFF CACHE BOOL "" FORCE)
set(EXPECTED_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(tl_expected
    GIT_REPOSITORY https://github.com/TartanLlama/expected.git
    GIT_TAG v1.3.1)

set(OPTIONAL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(tl_optional
    GIT_REPOSITORY https://github.com/TartanLlama/optional.git
    GIT_TAG v1.1.0)

set(MPARK_VARIANT_INCLUDE_TESTS "" CACHE STRING "" FORCE)
FetchContent_Declare(mpark_variant
    GIT_REPOSITORY https://github.com/mpark/variant.git
    GIT_TAG v1.4.0)

# convenience utility
FetchContent_Declare(pattern_matching
    GIT_REPOSITORY https://github.com/sergio-eld/pattern_matching.git
    GIT_TAG master)

FetchContent_MakeAvailable(tl_expected
    tl_optional
    mpark_variant
    pattern_matching)

add_library(std_compat INTERFACE)
target_link_libraries(std_compat INTERFACE
    tl::expected
    tl::optional
    mpark_variant
    eld::pattern_matching)

add_library(test.common.static_odr STATIC common/test_static_odr.cpp)
target_link_libraries(test.common.static_odr PRIVATE GTest::GTest std_compat)
omni_reflected_target(test.common.static_odr)

add_executable(test.common common/test_main.cpp common/test.cpp common/interface_test.cpp)
target_link_libraries(test.common PRIVATE GTest::GTest std_compat test.common.static_odr)
omni_reflected_target(test.common)

add_test(NAME test.common.run_omni
    COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" -t test.common.omni)

add_test(NAME test.common.build
    COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" -t test.common)

add_test(NAME test.common
    COMMAND test.common)

set_tests_properties(test.common.build PROPERTIES
    DEPENDS test.common.run_omni)

set_tests_properties(test.common PROPERTIES
    DEPENDS test.common.build)

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

