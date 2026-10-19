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
        COMMAND omni::ccdb_query "${_ccdb_query_db}" "${source}" ${ARGN})
endfunction()

function(_ccdb_query_fail name source)
    add_test(NAME "ccdb_query.${name}"
        COMMAND ${CMAKE_COMMAND}
            -D "CCDB_QUERY=$<TARGET_FILE:omni::ccdb_query>"
            -D "COMPILE_DB=${_ccdb_query_db}"
            -D "SOURCE=${source}"
            -D "EXPECT=${ARGV2}"
            -D "OUTPUT_CONTAINS=${ARGV3}"
            -P "${CMAKE_CURRENT_LIST_DIR}/expect_failure.cmake")
endfunction()

add_test(NAME ccdb_query.help
    COMMAND omni::ccdb_query --help)
set_tests_properties(ccdb_query.help PROPERTIES
    PASS_REGULAR_EXPRESSION
        "Usage: ccdb_query <compile_commands.json> <source.cpp> \\[output-contains\\]")

_ccdb_query_pass(unique "${_ccdb_query_unique}")
set_tests_properties(ccdb_query.unique PROPERTIES
    PASS_REGULAR_EXPRESSION "UNIQUE_TARGET")

add_test(NAME ccdb_query.backslash_argument
    COMMAND ${CMAKE_COMMAND}
        -D "CCDB_QUERY=$<TARGET_FILE:omni::ccdb_query>"
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
    COMMAND omni::ccdb_query
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

# A 256-character filename exceeds the component limit on the test filesystems.
string(REPEAT "x" 256 _ccdb_query_invalid_filename)
_ccdb_query_fail(invalid_source_path
    "${CMAKE_CURRENT_BINARY_DIR}/${_ccdb_query_invalid_filename}"
    "source does not exist")

file(CONFIGURE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/inferred-output.json"
    CONTENT [=[
[
  {
    "directory": "@CMAKE_CURRENT_BINARY_DIR@",
    "command": "c++ -DINFERRED_ALPHA -o \"/build/with space/alpha.dir/shared.cpp.o\" -c \"@_ccdb_query_shared@\"",
    "file": "@_ccdb_query_shared@"
  },
  {
    "directory": "@CMAKE_CURRENT_BINARY_DIR@",
    "arguments": ["c++", "-DINFERRED_BETA", "-o/build/beta.dir/shared.cpp.o", "-c", "@_ccdb_query_shared@"],
    "file": "@_ccdb_query_shared@"
  },
  {
    "directory": "@CMAKE_CURRENT_BINARY_DIR@",
    "arguments": ["c++", "-DEXPLICIT_OUTPUT", "-o", "/build/ignored.dir/shared.cpp.o", "-c", "@_ccdb_query_shared@"],
    "file": "@_ccdb_query_shared@",
    "output": "/build/explicit.dir/shared.cpp.o"
  },
  {
    "directory": "@CMAKE_CURRENT_BINARY_DIR@",
    "arguments": ["c++", "-DUNRELATED=/build/decoy.dir/shared.cpp.o", "-c", "@_ccdb_query_shared@"],
    "file": "@_ccdb_query_shared@"
  }
]
]=]
    @ONLY)
foreach(_case IN ITEMS alpha beta explicit)
    add_test(NAME ccdb_query.inferred_output.${_case}
        COMMAND omni::ccdb_query
            "${CMAKE_CURRENT_BINARY_DIR}/inferred-output.json"
            "${_ccdb_query_shared}" "/${_case}.dir/")
endforeach()
set_tests_properties(ccdb_query.inferred_output.alpha PROPERTIES
    PASS_REGULAR_EXPRESSION "INFERRED_ALPHA")
set_tests_properties(ccdb_query.inferred_output.beta PROPERTIES
    PASS_REGULAR_EXPRESSION "INFERRED_BETA")
set_tests_properties(ccdb_query.inferred_output.explicit PROPERTIES
    PASS_REGULAR_EXPRESSION "EXPLICIT_OUTPUT")

foreach(_case IN ITEMS ignored decoy ambiguous)
    if(_case STREQUAL "ambiguous")
        set(_selector "")
        set(_expected "ambiguous compile command")
    else()
        set(_selector "/${_case}.dir/")
        set(_expected "no compile command")
    endif()
    add_test(NAME ccdb_query.inferred_output.${_case}
        COMMAND ${CMAKE_COMMAND}
            -D "CCDB_QUERY=$<TARGET_FILE:omni::ccdb_query>"
            -D "COMPILE_DB=${CMAKE_CURRENT_BINARY_DIR}/inferred-output.json"
            -D "SOURCE=${_ccdb_query_shared}"
            -D "EXPECT=${_expected}"
            -D "OUTPUT_CONTAINS=${_selector}"
            -P "${CMAKE_CURRENT_LIST_DIR}/expect_failure.cmake")
endforeach()

# Output is optional: implicit object names and syntax-only commands are valid.
foreach(_case IN ITEMS implicit syntax_only)
    if(_case STREQUAL "implicit")
        set(_arguments "\"-c\"")
    else()
        set(_arguments "\"-fsyntax-only\"")
    endif()
    file(CONFIGURE
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${_case}.json"
        CONTENT [=[
[
  {
    "directory": "@CMAKE_CURRENT_BINARY_DIR@",
    "arguments": ["c++", "-DNO_EXPLICIT_OUTPUT", @_arguments@, "@_ccdb_query_unique@"],
    "file": "@_ccdb_query_unique@"
  }
]
]=]
        @ONLY)
    add_test(NAME ccdb_query.optional_output.${_case}
        COMMAND ${CMAKE_COMMAND}
            -D "CCDB_QUERY=$<TARGET_FILE:omni::ccdb_query>"
            -D "COMPILE_DB=${CMAKE_CURRENT_BINARY_DIR}/${_case}.json"
            -D "SOURCE=${_ccdb_query_unique}"
            -D "EXPECT_OUTPUT=NO_EXPLICIT_OUTPUT"
            -P "${CMAKE_CURRENT_LIST_DIR}/expect_silent_success.cmake")
    add_test(NAME ccdb_query.optional_output.${_case}_selector
        COMMAND ${CMAKE_COMMAND}
            -D "CCDB_QUERY=$<TARGET_FILE:omni::ccdb_query>"
            -D "COMPILE_DB=${CMAKE_CURRENT_BINARY_DIR}/${_case}.json"
            -D "SOURCE=${_ccdb_query_unique}"
            -D "EXPECT=no compile command"
            -D "OUTPUT_CONTAINS=unique.dir"
            -P "${CMAKE_CURRENT_LIST_DIR}/expect_failure.cmake")
endforeach()

add_test(NAME ccdb_query.optional_output.mixed_candidates
    COMMAND ${CMAKE_COMMAND}
        -D "CCDB_QUERY=$<TARGET_FILE:omni::ccdb_query>"
        -D "COMPILE_DB=${CMAKE_CURRENT_BINARY_DIR}/inferred-output.json"
        -D "SOURCE=${_ccdb_query_shared}"
        -D "OUTPUT_CONTAINS=/alpha.dir/"
        -D "EXPECT_OUTPUT=INFERRED_ALPHA"
        -P "${CMAKE_CURRENT_LIST_DIR}/expect_silent_success.cmake")
