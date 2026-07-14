cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_tests LANGUAGES C CXX)

enable_testing()

option(ENABLE_BENCH "Enable omnirefl benchmark targets" OFF)

if (NOT TARGET omni::tool)
    find_package(omnirefl CONFIG REQUIRED)
endif()

add_test(NAME omnirefl.cli.help COMMAND $<TARGET_FILE:omni::tool> --help)
set_tests_properties(omnirefl.cli.help PROPERTIES
    PASS_REGULAR_EXPRESSION
        "Usage: omnirefl -o <reflection.hpp> -c <source.cpp>")

# FIXME(high): expose `--version` as a tool option. It currently enters normal
# instrumentation parsing (and may fall through to compiler arguments), but it
# should print the package version and compiled commit identifier by itself.
add_test(NAME omnirefl.cli.version
    COMMAND $<TARGET_FILE:omni::tool> --version)
set_tests_properties(omnirefl.cli.version PROPERTIES
    DISABLED TRUE
    PASS_REGULAR_EXPRESSION "omnirefl [0-9]+\\.[0-9]+\\.[0-9]+")

if(DEFINED omnirefl_DIR)
    # FIXME(low): CMake escapes `$` as `$$` in compile_commands.json. The
    # package wrapper currently compares that spelling literally and emits a
    # false compiler-source mismatch warning, although generation succeeds.
    add_test(NAME omnirefl.regression.dollar_path_source
        COMMAND ${CMAKE_COMMAND}
            -D "FIXTURE=${CMAKE_CURRENT_LIST_DIR}/regressions/dollar_path"
            -D "ROOT=${CMAKE_CURRENT_BINARY_DIR}/regressions"
            -D "GENERATOR=${CMAKE_GENERATOR}"
            -D "CXX_COMPILER=${CMAKE_CXX_COMPILER}"
            -D "OMNIREFL_DIR=${omnirefl_DIR}"
            -P "${CMAKE_CURRENT_LIST_DIR}/regressions/dollar_path/run.cmake")
    set_tests_properties(omnirefl.regression.dollar_path_source PROPERTIES
        DISABLED TRUE)

    # FIXME(low): add_custom_command outputs are target-owned C++ sources, but
    # the wrapper filters their GENERATED property and reports that the target
    # has no C/C++ sources. Pre-generating the file is a workable alternative.
    add_test(NAME omnirefl.regression.generated_source
        COMMAND ${CMAKE_COMMAND}
            -D "FIXTURE=${CMAKE_CURRENT_LIST_DIR}/regressions/generated_source"
            -D "ROOT=${CMAKE_CURRENT_BINARY_DIR}/regressions/generated_source"
            -D "GENERATOR=${CMAKE_GENERATOR}"
            -D "CXX_COMPILER=${CMAKE_CXX_COMPILER}"
            -D "OMNIREFL_DIR=${omnirefl_DIR}"
            -P
                "${CMAKE_CURRENT_LIST_DIR}/regressions/generated_source/run.cmake")
    set_tests_properties(omnirefl.regression.generated_source PROPERTIES
        DISABLED TRUE)
endif()

set(_omnirefl_cli_compile_source -c)
if(MSVC)
    set(_omnirefl_cli_compile_source /c)
endif()

add_test(NAME omnirefl.cli.conflicting_compile_source
    COMMAND $<TARGET_FILE:omni::tool>
        --resource-dir "${omnirefl_RESOURCE_DIR}"
        --log-level silent
        -o conflicting-source.omnirefl.hpp
        -c "${CMAKE_CURRENT_LIST_DIR}/cli_source.cpp"
        -- "${CMAKE_CXX_COMPILER}"
        "${_omnirefl_cli_compile_source}"
        "${CMAKE_CURRENT_LIST_DIR}/cli_other_source.cpp")
set_tests_properties(omnirefl.cli.conflicting_compile_source PROPERTIES
    PASS_REGULAR_EXPRESSION
        "warning: compiler source after `--` differs from omnirefl `-c`"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

add_test(NAME omnirefl.cli.relative_output
    COMMAND $<TARGET_FILE:omni::tool>
        --resource-dir "${omnirefl_RESOURCE_DIR}"
        --log-level silent
        -o relative-output.omnirefl.hpp
        -c "${CMAKE_CURRENT_LIST_DIR}/cli_source.cpp"
        -- "${CMAKE_CXX_COMPILER}"
        "${_omnirefl_cli_compile_source}"
        "${CMAKE_CURRENT_LIST_DIR}/cli_source.cpp"
        -o ignored-object-file)
set_tests_properties(omnirefl.cli.relative_output PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

set(_compiler_response_file
    "${CMAKE_CURRENT_BINARY_DIR}/compiler-response-file.rsp")
file(WRITE "${_compiler_response_file}" "-DOMNI_RESPONSE_FILE_EXPANDED\n")

# FIXME(low): compiler response files are parsed as opaque input arguments, so
# frontend-relevant options inside @file never reach AST construction.
add_test(NAME omnirefl.regression.compiler_response_file
    COMMAND $<TARGET_FILE:omni::tool>
        --resource-dir "${omnirefl_RESOURCE_DIR}"
        --log-level silent
        -o compiler-response-file.omnirefl.hpp
        -c "${CMAKE_CURRENT_LIST_DIR}/regressions/compiler_response_file.cpp"
        -- "${CMAKE_CXX_COMPILER}" "@${_compiler_response_file}")
set_tests_properties(omnirefl.regression.compiler_response_file PROPERTIES
    DISABLED TRUE
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

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

    # Keep unresolved compile-time targets available without building them in
    # package tests. Their tool-run diagnostics remain covered.
    function(_omnirefl_disable_diagnostic_build name)
        set(_target "cpp_17.diagnostic.${name}")
        set_tests_properties("${_target}.build" PROPERTIES DISABLED TRUE)

        if(CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME)
            set_tests_properties("${_target}" PROPERTIES DISABLED TRUE)
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
        3
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
        "private_alias_shadows_public_field"
        regressions/private_alias_shadows_public_field.cpp
        "reflected types:")
    _omnirefl_disable_diagnostic_build(
        "private_alias_shadows_public_field")
    _omnirefl_diagnostic_tool_run(
        "public_nested_in_private_parent"
        regressions/public_nested_in_private_parent.cpp
        "reflected types:")
    _omnirefl_disable_diagnostic_build(
        "public_nested_in_private_parent")
    _omnirefl_diagnostic_tool_run(
        "private_nested_type_through_public_base"
        regressions/private_nested_type_through_public_base.cpp
        "reflected types:")
    _omnirefl_disable_diagnostic_build(
        "private_nested_type_through_public_base")
    _omnirefl_diagnostic_tool_run(
        "public_nested_direct_input"
        regressions/public_nested_direct_input.cpp
        "reflected types: 2")
    _omnirefl_diagnostic_tool_run(
        "variant_named_record_is_reflected"
        regressions/variant_named_record_is_reflected.cpp
        "reflected types:")
    _omnirefl_diagnostic_tool_run(
        "inline_namespace"
        regressions/inline_namespace.cpp
        "reflected types: 1")
    _omnirefl_disable_diagnostic_build(
        "inline_namespace")
    _omnirefl_diagnostic_tool_run(
        "deprecated_public_field"
        regressions/deprecated_public_field.cpp
        "reflected types: 1")
    target_compile_options(
        cpp_17.diagnostic.deprecated_public_field PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Werror=deprecated-declarations>
        $<$<CXX_COMPILER_ID:MSVC>:/we4996>)
    _omnirefl_disable_diagnostic_build(
        "deprecated_public_field")
    _omnirefl_diagnostic_tool_run(
        "virtual_diamond_fields"
        regressions/virtual_diamond_fields.cpp
        "reflected types: 4")
    _omnirefl_diagnostic_tool_run(
        "anonymous_union_members"
        regressions/anonymous_union_members.cpp
        "unsupported reflection dependency")
    _omnirefl_disable_diagnostic_build(
        "anonymous_union_members")
    _omnirefl_diagnostic_tool_run(
        "field_set_value_assignability"
        regressions/field_set_value_assignability.cpp
        "reflected types: 2")
    _omnirefl_diagnostic_tool_run(
        "packed_field_reference"
        regressions/packed_field_reference.cpp
        "reflected types: 1")
    _omnirefl_disable_diagnostic_build(
        "packed_field_reference")
    _omnirefl_diagnostic_tool_run(
        "nested_template_field_name"
        regressions/nested_template_field_name.cpp
        "reflected types:")
    _omnirefl_diagnostic_tool_run(
        "volatile_binding_qualifiers"
        regressions/volatile_binding_qualifiers.cpp
        "reflected types: 1")
    _omnirefl_disable_diagnostic_build(
        "volatile_binding_qualifiers")
    _omnirefl_diagnostic_tool_run(
        "variadic_primary_template"
        regressions/variadic_primary_template.cpp
        "reflected types: 1")
    _omnirefl_disable_diagnostic_build(
        "variadic_primary_template")
    _omnirefl_diagnostic_tool_run(
        "dependent_nontype_template"
        regressions/dependent_nontype_template.cpp
        "reflected types: 1")
    _omnirefl_disable_diagnostic_build(
        "dependent_nontype_template")
    _omnirefl_diagnostic_tool_run(
        "multiple_primary_template_bases"
        regressions/multiple_primary_template_bases.cpp
        "reflected types:")
    _omnirefl_disable_diagnostic_build(
        "multiple_primary_template_bases")
    _omnirefl_diagnostic_tool_run(
        "template_template_argument"
        regressions/template_template_argument.cpp
        "reflected types:")
    _omnirefl_disable_diagnostic_build(
        "template_template_argument")
    set_tests_properties(
        "cpp_17.diagnostic.template_template_argument.run_omni"
        PROPERTIES DISABLED TRUE)
    _omnirefl_diagnostic_tool_run(
        "mixed_language_target"
        "regressions/mixed_language_target.cpp;regressions/mixed_language_helper.c"
        "reflected types:")
    _omnirefl_disable_diagnostic_build(
        "mixed_language_target")
    _omnirefl_diagnostic_tool_run(
        "ccdb_backslash_definition"
        regressions/ccdb_backslash_definition.cpp
        "reflected types:")
    target_compile_definitions(
        cpp_17.diagnostic.ccdb_backslash_definition PRIVATE
        "OMNI_OPTION_TEXT=R\"(left\\right)\"")
    _omnirefl_disable_diagnostic_build(
        "ccdb_backslash_definition")
    set_tests_properties(
        "cpp_17.diagnostic.ccdb_backslash_definition.run_omni"
        PROPERTIES DISABLED TRUE)

    function(_omnirefl_disabled_driver_option name selector)
        set(_target "cpp_17.diagnostic.${name}")
        _omnirefl_diagnostic_tool_run(
            "${name}"
            regressions/driver_option_fidelity.cpp
            "reflected types:")
        target_compile_definitions(${_target} PRIVATE "${selector}=1")
        target_compile_options(${_target} PRIVATE ${ARGN})
        _omnirefl_disable_diagnostic_build("${name}")
        set_tests_properties("${_target}.run_omni" PROPERTIES DISABLED TRUE)
    endfunction()

    _omnirefl_disabled_driver_option(
        "driver_option_imacros"
        OMNI_REGRESSION_IMACROS
        -imacros
        "${CMAKE_CURRENT_LIST_DIR}/regressions/driver_imacros_config.h")
    _omnirefl_disabled_driver_option(
        "driver_option_pthread"
        OMNI_REGRESSION_PTHREAD
        -pthread)
    _omnirefl_disabled_driver_option(
        "driver_option_openmp"
        OMNI_REGRESSION_OPENMP
        -fopenmp)
    _omnirefl_disabled_driver_option(
        "driver_option_no_exceptions"
        OMNI_REGRESSION_NO_EXCEPTIONS
        -fno-exceptions)
    _omnirefl_disabled_driver_option(
        "driver_option_no_rtti"
        OMNI_REGRESSION_NO_RTTI
        -fno-rtti)
    _omnirefl_disabled_driver_option(
        "driver_option_short_wchar"
        OMNI_REGRESSION_SHORT_WCHAR
        -fshort-wchar)
    _omnirefl_disabled_driver_option(
        "driver_option_unsigned_char"
        OMNI_REGRESSION_UNSIGNED_CHAR
        -funsigned-char)
    _omnirefl_disabled_driver_option(
        "driver_option_pack_struct"
        OMNI_REGRESSION_PACK_STRUCT
        -fpack-struct=1)
    _omnirefl_disabled_driver_option(
        "driver_option_no_operator_names"
        OMNI_REGRESSION_NO_OPERATOR_NAMES
        -fno-operator-names)
    _omnirefl_disabled_driver_option(
        "driver_option_avx2"
        OMNI_REGRESSION_AVX2
        -mavx2)
    _omnirefl_disabled_driver_option(
        "driver_option_no_access_control"
        OMNI_REGRESSION_NO_ACCESS_CONTROL
        -fno-access-control)
    _omnirefl_disabled_driver_option(
        "driver_option_no_sized_deallocation"
        OMNI_REGRESSION_NO_SIZED_DEALLOCATION
        -fno-sized-deallocation)

    foreach(_target IN ITEMS
            regression_ccdb_output_suffix
            myregression_ccdb_output_suffix)
        add_executable(${_target} EXCLUDE_FROM_ALL example_test.cpp)
        set_target_properties(${_target} PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO)
        target_link_libraries(${_target} PRIVATE
            GTest::GTest
            gtest_main
            std_compat)
        omni_reflected_target(${_target} LOG_LEVEL warning)
    endforeach()

    add_test(NAME "cpp_17.diagnostic.ccdb_output_suffix.run_omni"
        COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}"
            -t "regression_ccdb_output_suffix.omni" --parallel 1)

    add_test(NAME "cpp_17.diagnostic.ccdb_output_suffix.build"
        COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}"
            -t "regression_ccdb_output_suffix" --parallel)
    set_tests_properties(
        "cpp_17.diagnostic.ccdb_output_suffix.build"
        PROPERTIES
            DEPENDS "cpp_17.diagnostic.ccdb_output_suffix.run_omni")

    if(CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME)
        add_test(NAME "cpp_17.diagnostic.ccdb_output_suffix"
            COMMAND regression_ccdb_output_suffix)
        set_tests_properties(
            "cpp_17.diagnostic.ccdb_output_suffix"
            PROPERTIES
                DEPENDS "cpp_17.diagnostic.ccdb_output_suffix.build")
    endif()
endif()

if("20" IN_LIST _supported_cpp_standards)
    function(_omnirefl_disabled_configure_regression name)
        set(_source_dir
            "${CMAKE_CURRENT_LIST_DIR}/regressions/cmake/${name}")
        set(_build_dir
            "${CMAKE_CURRENT_BINARY_DIR}/regressions/cmake/${name}")

        add_test(NAME "cpp_20.regression.${name}.configure"
            COMMAND ${CMAKE_COMMAND}
                -S "${_source_dir}"
                -B "${_build_dir}"
                -GNinja
                -D "omnirefl_DIR=${omnirefl_DIR}")
        set_tests_properties("cpp_20.regression.${name}.configure"
            PROPERTIES DISABLED TRUE)
    endfunction()

    _omnirefl_disabled_configure_regression(uppercase_cpp_source)
    _omnirefl_disabled_configure_regression(generator_expression_source)

    set(_no_char8_target "cpp_20.diagnostic.driver_option_no_char8_t")
    add_executable(${_no_char8_target} EXCLUDE_FROM_ALL
        regressions/driver_option_fidelity.cpp)
    set_target_properties(${_no_char8_target} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)
    target_compile_definitions(${_no_char8_target} PRIVATE
        OMNI_REGRESSION_NO_CHAR8_T=1)
    target_compile_options(${_no_char8_target} PRIVATE -fno-char8_t)
    target_link_libraries(${_no_char8_target} PRIVATE
        GTest::GTest
        gtest_main
        std_compat)
    omni_reflected_target(${_no_char8_target} LOG_LEVEL warning)

    add_test(NAME "${_no_char8_target}.run_omni"
        COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}"
            -t "${_no_char8_target}.omni" --parallel 1)
    set_tests_properties("${_no_char8_target}.run_omni"
        PROPERTIES DISABLED TRUE)

    set(_concept_sfinae_target
        "cpp_20.regression.binding_concept_sfinae")
    add_executable(${_concept_sfinae_target} EXCLUDE_FROM_ALL
        regressions/binding_concept_sfinae.cpp)
    set_target_properties(${_concept_sfinae_target} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)
    target_link_libraries(${_concept_sfinae_target} PRIVATE
        GTest::GTest
        gtest_main
        std_compat)
    omni_reflected_target(${_concept_sfinae_target} LOG_LEVEL warning)

    add_test(NAME "${_concept_sfinae_target}.run_omni"
        COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}"
            -t "${_concept_sfinae_target}.omni" --parallel 1)
    add_test(NAME "${_concept_sfinae_target}.build"
        COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}"
            -t "${_concept_sfinae_target}" --parallel)
    add_test(NAME "${_concept_sfinae_target}"
        COMMAND ${_concept_sfinae_target})
    set_tests_properties(
        "${_concept_sfinae_target}.run_omni"
        "${_concept_sfinae_target}.build"
        "${_concept_sfinae_target}"
        PROPERTIES DISABLED TRUE)

    set(_constrained_template_target
        "cpp_20.regression.constrained_primary_template")
    add_executable(${_constrained_template_target} EXCLUDE_FROM_ALL
        regressions/constrained_primary_template.cpp)
    set_target_properties(${_constrained_template_target} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)
    target_link_libraries(${_constrained_template_target} PRIVATE
        GTest::GTest
        gtest_main
        std_compat)
    omni_reflected_target(${_constrained_template_target} LOG_LEVEL warning)

    add_test(NAME "${_constrained_template_target}.run_omni"
        COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}"
            -t "${_constrained_template_target}.omni" --parallel 1)
    add_test(NAME "${_constrained_template_target}.build"
        COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}"
            -t "${_constrained_template_target}" --parallel)
    add_test(NAME "${_constrained_template_target}"
        COMMAND ${_constrained_template_target})
    set_tests_properties(
        "${_constrained_template_target}.build"
        "${_constrained_template_target}"
        PROPERTIES DISABLED TRUE)
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
