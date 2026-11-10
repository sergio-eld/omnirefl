cmake_minimum_required(VERSION 3.18.2)
find_package(Git REQUIRED QUIET)

# emplace() calls value() after successful construction. Its explicit throw
# fails under -fno-exceptions even when constructing T cannot throw.
set(_patch "${CMAKE_CURRENT_LIST_DIR}/tl_optional-emplace.patch")
execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --check "${_patch}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE _result
    ERROR_VARIABLE _error)
if(_result EQUAL 0)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" apply "${_patch}"
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE _result
        ERROR_VARIABLE _error)
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR "Cannot patch tl::optional emplacement: ${_error}")
    endif()
else()
    # Accept reapplication only when both hunks are already present.
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${_patch}"
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE _result
        ERROR_VARIABLE _reverse_error)
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR
            "Cannot patch tl::optional emplacement: unexpected content\n${_error}${_reverse_error}")
    endif()
endif()
