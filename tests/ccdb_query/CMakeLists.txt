cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "" FORCE)
add_subdirectory(dummy dummy-build)

set(_ccdb_query_db "${CMAKE_BINARY_DIR}/compile_commands.json")
set(_ccdb_query_unique "${CMAKE_CURRENT_LIST_DIR}/dummy/unique.cpp")
set(_ccdb_query_shared "${CMAKE_CURRENT_LIST_DIR}/dummy/shared.cpp")
set(_ccdb_query_missing "${CMAKE_CURRENT_LIST_DIR}/dummy/missing.cpp")
set(_ccdb_query_backslash_db_dir
    "${CMAKE_CURRENT_BINARY_DIR}/backslash-output")

file(MAKE_DIRECTORY "${_ccdb_query_backslash_db_dir}")
file(CONFIGURE
    OUTPUT "${_ccdb_query_backslash_db_dir}/compile_commands.json"
    CONTENT [=[
[
  {
    "directory": "@CMAKE_CURRENT_BINARY_DIR@",
    "arguments": ["c++", "-DALPHA_TARGET", "-c", "@_ccdb_query_shared@"],
    "file": "@_ccdb_query_shared@",
    "output": "C:\\build\\alpha.dir\\shared.cpp.o"
  }
]
]=]
    @ONLY)

function(_ccdb_query_pass name source)
    add_test(NAME "ccdb_query.${name}"
        COMMAND ccdb_query "${_ccdb_query_db}" "${source}" ${ARGN})
endfunction()

function(_ccdb_query_fail name source)
    add_test(NAME "ccdb_query.${name}"
        COMMAND ${CMAKE_COMMAND}
            -D "CCDB_QUERY=$<TARGET_FILE:ccdb_query>"
            -D "COMPILE_DB=${_ccdb_query_db}"
            -D "SOURCE=${source}"
            -D "EXPECT=${ARGV2}"
            -D "OUTPUT_CONTAINS=${ARGV3}"
            -P "${CMAKE_CURRENT_LIST_DIR}/expect_failure.cmake")
endfunction()

add_test(NAME ccdb_query.help
    COMMAND ccdb_query --help)
set_tests_properties(ccdb_query.help PROPERTIES
    PASS_REGULAR_EXPRESSION
        "Usage: ccdb_query <compile_commands.json> <source.cpp> \\[output-contains\\]")

_ccdb_query_pass(unique "${_ccdb_query_unique}")
set_tests_properties(ccdb_query.unique PROPERTIES
    PASS_REGULAR_EXPRESSION "UNIQUE_TARGET")

add_test(NAME ccdb_query.backslash_argument
    COMMAND ${CMAKE_COMMAND}
        -D "CCDB_QUERY=$<TARGET_FILE:ccdb_query>"
        -D "COMPILE_DB=${_ccdb_query_db}"
        -D "SOURCE=${_ccdb_query_unique}"
        -P "${CMAKE_CURRENT_LIST_DIR}/expect_backslash.cmake")

_ccdb_query_pass(alpha "${_ccdb_query_shared}" "alpha.dir")
set_tests_properties(ccdb_query.alpha PROPERTIES
    PASS_REGULAR_EXPRESSION "ALPHA_TARGET")

_ccdb_query_pass(alpha_slash "${_ccdb_query_shared}" "/alpha.dir/")
set_tests_properties(ccdb_query.alpha_slash PROPERTIES
    PASS_REGULAR_EXPRESSION "ALPHA_TARGET")

_ccdb_query_pass(alpha_backslash "${_ccdb_query_shared}" "\\alpha.dir\\")
set_tests_properties(ccdb_query.alpha_backslash PROPERTIES
    PASS_REGULAR_EXPRESSION "ALPHA_TARGET")

add_test(NAME ccdb_query.backslash_output
    COMMAND ccdb_query
        "${_ccdb_query_backslash_db_dir}/compile_commands.json"
        "${_ccdb_query_shared}"
        "/alpha.dir/")
set_tests_properties(ccdb_query.backslash_output PROPERTIES
    PASS_REGULAR_EXPRESSION "ALPHA_TARGET")

_ccdb_query_pass(beta "${_ccdb_query_shared}" "beta.dir")
set_tests_properties(ccdb_query.beta PROPERTIES
    PASS_REGULAR_EXPRESSION "BETA_TARGET")

_ccdb_query_fail(ambiguous
    "${_ccdb_query_shared}"
    "ambiguous compile command")

_ccdb_query_fail(missing_discriminator
    "${_ccdb_query_shared}"
    "no compile command"
    "missing.dir")

_ccdb_query_fail(missing_source
    "${_ccdb_query_missing}"
    "source does not exist")
