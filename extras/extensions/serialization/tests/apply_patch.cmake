cmake_minimum_required(VERSION 3.18.2)
file(MAKE_DIRECTORY "${TEST_ROOT}/src/c4/std")
set(_original "#if (C4_CPP >= 20) || defined(__DOXYGEN__)\n#include <span>\n#endif\n")
set(_expected "#if (C4_CPP >= 20 && __has_include(<span>)) || defined(__DOXYGEN__)\n#include <span>\n#endif\n")
foreach(_header span.hpp span_fwd.hpp)
    file(WRITE "${TEST_ROOT}/src/c4/std/${_header}" "${_original}")
endforeach()

foreach(_attempt RANGE 1 2)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${TEST_ROOT}"
            -P "${PATCH_SCRIPT}"
        RESULT_VARIABLE _result)
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR "Patch application ${_attempt} failed")
    endif()
endforeach()
foreach(_header span.hpp span_fwd.hpp)
    file(READ "${TEST_ROOT}/src/c4/std/${_header}" _content)
    if(NOT _content STREQUAL _expected)
        message(FATAL_ERROR "Patch did not produce the expected content in ${_header}")
    endif()
endforeach()

file(WRITE "${TEST_ROOT}/unrelated.txt" "unrelated\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${TEST_ROOT}"
        -P "${PATCH_SCRIPT}"
    RESULT_VARIABLE _result)
file(READ "${TEST_ROOT}/src/c4/std/span.hpp" _content)
file(READ "${TEST_ROOT}/unrelated.txt" _unrelated)
if(NOT _result EQUAL 0 OR NOT _content STREQUAL _expected
        OR NOT _unrelated STREQUAL "unrelated\n")
    message(FATAL_ERROR "Unrelated changes were not preserved")
endif()

file(WRITE "${TEST_ROOT}/src/c4/std/span.hpp" "incompatible\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${TEST_ROOT}"
        -P "${PATCH_SCRIPT}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _error)
file(READ "${TEST_ROOT}/src/c4/std/span.hpp" _content)
if(_result EQUAL 0 OR NOT _content STREQUAL "incompatible\n"
        OR NOT _error MATCHES "Cannot patch c4core span guard")
    message(FATAL_ERROR "An incompatible change was ignored or modified: ${_out}${_error}")
endif()
