cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_tests LANGUAGES CXX)

enable_testing()

option(ENABLE_BENCH "Enable omnirefl benchmark targets" OFF)

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

if("17" IN_LIST _supported_cpp_standards)
    function(_omnirefl_negative_tool_run name source expected_diagnostics)
        set(_target "cpp_17.negative.${name}")
        set(_expected_message "${ARGV3}")
        set(_diagnostic_regex "${ARGV4}")
        set(_expected_message_2 "${ARGV5}")
        if("" STREQUAL "${_expected_message}")
            set(_expected_message "invalid reflection queries")
        endif()
        if("" STREQUAL "${_diagnostic_regex}")
            set(_diagnostic_regex ":[0-9]+:[0-9]+: reflection query")
        endif()

        add_executable(${_target} EXCLUDE_FROM_ALL "${source}")
        set_target_properties(${_target} PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
        )
        target_compile_definitions(${_target} PRIVATE CXX_STANDARD=17)
        target_link_libraries(${_target} PRIVATE std_compat)
        if("NO_INDEX" STREQUAL "${ARGV6}")
            omni_reflected_target(${_target})
        else()
            omni_reflected_target(${_target} ENABLE_INDEX_MODE)
        endif()

        add_test(NAME "${_target}.run_omni"
            COMMAND ${CMAKE_COMMAND}
                -D "BUILD_DIR=${CMAKE_BINARY_DIR}"
                -D "TARGET=${_target}.omni"
                -D "EXPECTED_DIAGNOSTICS=${expected_diagnostics}"
                -D "EXPECTED_MESSAGE=${_expected_message}"
                -D "DIAGNOSTIC_REGEX=${_diagnostic_regex}"
                -D "EXPECTED_MESSAGE_2=${_expected_message_2}"
                -D "UNEXPECTED_MESSAGE=${ARGV7}"
                -P "${CMAKE_CURRENT_LIST_DIR}/expect_negative_tool_run.cmake")
    endfunction()

    function(_omnirefl_diagnostic_tool_run name source expected_message)
        set(_target "cpp_17.diagnostic.${name}")
        set(_expected_message_2 "${ARGV3}")
        set(_log_level "${ARGV4}")
        if("" STREQUAL "${_log_level}")
            set(_log_level info)
        endif()

        add_executable(${_target} EXCLUDE_FROM_ALL "${source}")
        set_target_properties(${_target} PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
        )
        target_compile_definitions(${_target} PRIVATE CXX_STANDARD=17)
        target_link_libraries(${_target} PRIVATE
            GTest::GTest
            gtest_main
            std_compat)
        omni_reflected_target(${_target} LOG_LEVEL ${_log_level})

        add_test(NAME "${_target}.run_omni"
            COMMAND ${CMAKE_COMMAND}
                -D "BUILD_DIR=${CMAKE_BINARY_DIR}"
                -D "TARGET=${_target}.omni"
                -D "EXPECTED_MESSAGE=${expected_message}"
                -D "EXPECTED_MESSAGE_2=${_expected_message_2}"
                -P "${CMAKE_CURRENT_LIST_DIR}/expect_tool_run.cmake")

        add_test(NAME "${_target}.build"
            COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}"
                -t "${_target}" --parallel)
        set_tests_properties("${_target}.build" PROPERTIES
            DEPENDS "${_target}.run_omni")

        if(CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME)
            add_test(NAME "${_target}" COMMAND ${_target})
            set_tests_properties("${_target}" PROPERTIES
                DEPENDS "${_target}.build")
        endif()
    endfunction()

    _omnirefl_negative_tool_run(
        "query_out_of_scope"
        negative_run/query_out_of_scope.cpp
        1)
    _omnirefl_negative_tool_run(
        "query_non_template_visitor"
        negative_run/query_non_template_visitor.cpp
        2
        "invalid reflection queries"
        ":[0-9]+:[0-9]+: reflection query"
        "call stack:")
    _omnirefl_negative_tool_run(
        "query_lambda_deduced_return"
        negative_run/query_lambda_deduced_return.cpp
        2
        "invalid reflection queries"
        ":[0-9]+:[0-9]+: reflection query"
        "call stack:")
    _omnirefl_negative_tool_run(
        "query_multiple"
        negative_run/query_multiple.cpp
        7)
    _omnirefl_negative_tool_run(
        "reflected_call_args"
        negative_run/reflected_call_args.cpp
        4
        "invalid reflected_call arguments"
        ":[0-9]+:[0-9]+: reflected_call argument")
    _omnirefl_negative_tool_run(
        "reflected_call_partial_specialization"
        negative_run/reflected_call_partial_specialization.cpp
        1
        "partial template specialization"
        ":[0-9]+:[0-9]+: reflected_call argument")
    _omnirefl_negative_tool_run(
        "reflected_call_forward_declaration"
        negative_run/reflected_call_forward_declaration.cpp
        1
        "forward declarations without definitions"
        ":[0-9]+:[0-9]+: reflected_call argument")
    _omnirefl_negative_tool_run(
        "reflected_call_template_callstack"
        negative_run/reflected_call_template_callstack.cpp
        1
        "instantiated from"
        ":[0-9]+:[0-9]+: reflected_call argument")
    _omnirefl_negative_tool_run(
        "reflected_call_local_unnamed"
        negative_run/reflected_call_local_unnamed.cpp
        3
        "local types are not supported"
        ":[0-9]+:[0-9]+: reflected_call argument"
        "unnamed types are not supported"
        NO_INDEX
        "ENABLE_INDEX_MODE|indexed support|index mode")
    _omnirefl_negative_tool_run(
        "reflected_call_unscoped_enum"
        negative_run/reflected_call_unscoped_enum.cpp
        1
        "enum cannot be forward-declared"
        ":[0-9]+:[0-9]+: reflected_call argument"
        ""
        NO_INDEX
        "ENABLE_INDEX_MODE|indexed support|index mode")
    _omnirefl_negative_tool_run(
        "reflected_call_explicit_specialization"
        negative_run/reflected_call_explicit_specialization.cpp
        1
        "explicit specializations are not supported"
        ":[0-9]+:[0-9]+: reflected_call argument")
    _omnirefl_negative_tool_run(
        "reflected_call_nested_template_parent"
        negative_run/reflected_call_nested_template_parent.cpp
        1
        "records nested inside template records are not supported"
        ":[0-9]+:[0-9]+: reflected_call argument")
    _omnirefl_negative_tool_run(
        "query_and_reflected_call_arg"
        negative_run/query_and_reflected_call_arg.cpp
        2
        "invalid reflection queries"
        ":[0-9]+:[0-9]+: (reflection query|reflected_call argument)"
        "invalid reflected_call arguments")
    _omnirefl_negative_tool_run(
        "reflected_call_non_template_nested_in_template"
        negative_run/reflected_call_non_template_nested_in_template.cpp
        1
        "records nested inside template records are not supported"
        ":[0-9]+:[0-9]+: reflected_call argument")
    _omnirefl_diagnostic_tool_run(
        "reflected_dependency_unnamed_nested"
        diagnostic_run/reflected_dependency_unnamed_nested.cpp
        "unsupported reflection dependency .* skipped")
    _omnirefl_diagnostic_tool_run(
        "std_public_base"
        diagnostic_run/std_public_base.cpp
        "standard-library public base 'std::enable_shared_from_this"
        ""
        warning)
    _omnirefl_diagnostic_tool_run(
        "unsupported_public_base"
        diagnostic_run/unsupported_public_base.cpp
        "unsupported public base dependency .* skipped"
        "Inherited fields from this base will not be reflected")
    _omnirefl_diagnostic_tool_run(
        "partial_specialization_does_not_prune_primary"
        regressions/partial_specialization_does_not_prune_primary.cpp
        "partial template specializations are not supported"
        "reflected types: 2")
    _omnirefl_diagnostic_tool_run(
        "partial_specialization_base_collision"
        regressions/partial_specialization_base_collision.cpp
        "unsupported public base dependency"
        "reflected types: 3")
    _omnirefl_diagnostic_tool_run(
        "skipped_dependency_prunes_orphans"
        regressions/skipped_dependency_prunes_orphans.cpp
        "partial template specializations are not supported"
        "reflected types: 1")
    _omnirefl_diagnostic_tool_run(
        "private_nested_type_is_reflectable"
        regressions/private_nested_type_is_reflectable.cpp
        "reflected types: 2")
    _omnirefl_diagnostic_tool_run(
        "private_nested_type_through_wrappers"
        regressions/private_nested_type_through_wrappers.cpp
        "reflected types: 4")
    _omnirefl_diagnostic_tool_run(
        "public_nested_direct_input"
        regressions/public_nested_direct_input.cpp
        "reflected types: 2")
    _omnirefl_diagnostic_tool_run(
        "variant_named_record_is_reflected"
        regressions/variant_named_record_is_reflected.cpp
        "reflected types:")
endif()

list(REMOVE_DUPLICATES _supported_cpp_standards)
list(SORT _supported_cpp_standards)

# C++26 has standard reflection. Keep omnirefl release/package validation on the
# pre-reflection language standards it targets.
list(REMOVE_ITEM _supported_cpp_standards "26")

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

if(ENABLE_BENCH)
    set(_benchmark_cpp_standard "")
    foreach(_n IN LISTS _supported_cpp_standards)
        if(_n LESS_EQUAL 23)
            set(_benchmark_cpp_standard "${_n}")
        endif()
    endforeach()

    if(NOT _benchmark_cpp_standard)
        message(FATAL_ERROR "omnirefl: no C++ standard up to C++23 is available for benchmark.baseline")
    endif()

    add_library(benchmark.baseline STATIC baseline_test.cpp)
    set_target_properties(benchmark.baseline PROPERTIES
        CXX_STANDARD ${_benchmark_cpp_standard}
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )

    target_compile_definitions(benchmark.baseline PRIVATE CXX_STANDARD=${_benchmark_cpp_standard})
    target_link_libraries(benchmark.baseline PRIVATE GTest::GTest std_compat)
    omni_reflected_target(benchmark.baseline ENABLE_INDEX_MODE TIMINGS LOG_LEVEL silent)
endif()

set(_has_cxx20_reflected_guide_features OFF)
if("20" IN_LIST _supported_cpp_standards)
    file(WRITE
        "${CMAKE_CURRENT_BINARY_DIR}/check_cxx20_reflected_guide_features.cpp"
        [[
#if !defined(__cpp_concepts) || __cpp_concepts < 201907L
#  error "C++20 concepts are required"
#endif

#if !defined(__cpp_generic_lambdas) || __cpp_generic_lambdas < 201707L
#  error "explicit template parameter lists for lambdas are required"
#endif

static_assert(201907L <= __cpp_concepts);
static_assert(201707L <= __cpp_generic_lambdas);

int main() {
  return 0;
}
]])
    try_compile(_has_cxx20_reflected_guide_features
        "${CMAKE_CURRENT_BINARY_DIR}/check_cxx20_reflected_guide_features"
        "${CMAKE_CURRENT_BINARY_DIR}/check_cxx20_reflected_guide_features.cpp"
        CMAKE_FLAGS
            "-DCMAKE_CXX_STANDARD=20"
            "-DCMAKE_CXX_STANDARD_REQUIRED=ON"
            "-DCMAKE_CXX_EXTENSIONS=OFF"
        OUTPUT_VARIABLE _cxx20_reflected_guide_check_output)

    if(NOT _has_cxx20_reflected_guide_features)
        message(STATUS
            "omnirefl: skipping C++20 examples: required C++20 language features are not available")
        message(VERBOSE "${_cxx20_reflected_guide_check_output}")
    endif()
endif()

if(_has_cxx20_reflected_guide_features)
    add_subdirectory(example)
    add_subdirectory(comprehensive_guide)
endif()

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
        baseline_test.cpp
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
        COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" -t "${_test_tgt}" --parallel)

    set_tests_properties("${cpp_prefix}.test.build" PROPERTIES
        DEPENDS "${cpp_prefix}.test.run_omni")

    if(CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME)
        add_test(NAME "${cpp_prefix}.test" COMMAND ${_test_tgt})
        set_tests_properties("${cpp_prefix}.test" PROPERTIES
            DEPENDS "${cpp_prefix}.test.build")
    endif()
endforeach()
