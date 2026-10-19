foreach(required ROOT FIXTURE DEPENDENCY)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

# Copy inputs because installed fixtures may be read-only.
set(source_dir "${ROOT}/source")
file(REMOVE_RECURSE "${ROOT}")
file(MAKE_DIRECTORY "${source_dir}")
file(COPY "${FIXTURE}/" DESTINATION "${source_dir}")
set(FIXTURE "${source_dir}")
set(ROOT "${ROOT}/build")
set(EXECUTABLES incremental_build)
set(REQUIRED_REFLECTION_STEMS main)
include("${CMAKE_CURRENT_LIST_DIR}/run_project.cmake")

file(GLOB reflection_headers "${ROOT}/omni_incremental_build/*.omnirefl.hpp")
list(LENGTH reflection_headers header_count)
if(NOT 1 EQUAL header_count)
    message(FATAL_ERROR "Expected one generated header: ${reflection_headers}")
endif()
list(GET reflection_headers 0 reflection_header)
set(executable "${ROOT}/incremental_build${EXECUTABLE_SUFFIX}")

file(TIMESTAMP "${reflection_header}" header_before UTC)
file(TIMESTAMP "${executable}" executable_before UTC)
# CMake's portable timestamp format has second precision.
execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 1)

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${ROOT}" --parallel
    RESULT_VARIABLE unchanged_result
    OUTPUT_VARIABLE unchanged_out
    ERROR_VARIABLE unchanged_err)
if(NOT 0 EQUAL unchanged_result)
    message(FATAL_ERROR "Unchanged build failed:\n${unchanged_out}\n${unchanged_err}")
endif()
file(TIMESTAMP "${reflection_header}" header_after UTC)
if(NOT header_before STREQUAL header_after)
    message(FATAL_ERROR
        "Unchanged build regenerated reflection:\n${unchanged_out}\n${unchanged_err}")
endif()
file(TIMESTAMP "${executable}" executable_after UTC)
if(NOT executable_before STREQUAL executable_after)
    message(FATAL_ERROR
        "Unchanged build relinked the executable:\n${unchanged_out}\n${unchanged_err}")
endif()

file(READ "${source_dir}/${DEPENDENCY}" dependency_source)
string(REPLACE "};" "  int added;\n};" dependency_source "${dependency_source}")
file(WRITE "${source_dir}/${DEPENDENCY}" "${dependency_source}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${ROOT}" --parallel
    RESULT_VARIABLE changed_result
    OUTPUT_VARIABLE changed_out
    ERROR_VARIABLE changed_err)
if(NOT 0 EQUAL changed_result)
    message(FATAL_ERROR "Header-change build failed:\n${changed_out}\n${changed_err}")
endif()
file(TIMESTAMP "${reflection_header}" header_changed UTC)
if(header_before STREQUAL header_changed)
    message(FATAL_ERROR "Changing ${DEPENDENCY} did not regenerate reflection")
endif()
if(NOT CROSSCOMPILING)
    execute_process(
        COMMAND "${executable}"
        RESULT_VARIABLE run_result
        OUTPUT_VARIABLE field_count
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT 0 EQUAL run_result)
        message(FATAL_ERROR "Updated executable exited with code ${run_result}")
    endif()
    if(NOT "3" STREQUAL field_count)
        message(FATAL_ERROR "Expected three reflected fields, got '${field_count}'")
    endif()
endif()

file(REMOVE "${reflection_header}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${ROOT}" --parallel
    RESULT_VARIABLE recovery_result
    OUTPUT_VARIABLE recovery_out
    ERROR_VARIABLE recovery_err)
if(NOT 0 EQUAL recovery_result)
    message(FATAL_ERROR
        "Missing-header build failed:\n${recovery_out}\n${recovery_err}")
endif()
if(NOT EXISTS "${reflection_header}")
    message(FATAL_ERROR "Build did not restore the deleted reflection header")
endif()
