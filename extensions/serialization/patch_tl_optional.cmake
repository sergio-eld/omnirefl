cmake_minimum_required(VERSION 3.18.2)
find_package(Git REQUIRED QUIET)

# FetchContent may live inside the caller's checkout. Stop repository discovery
# before its parent so git apply cannot silently skip dependency-relative paths.
get_filename_component(_source_parent "${SOURCE_DIR}/.." REALPATH)
# PATCH_NAME is explicit so FetchContent notices changes to the patch set.
set(_patch "${CMAKE_CURRENT_LIST_DIR}/${PATCH_NAME}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "GIT_CEILING_DIRECTORIES=${_source_parent}"
        "${GIT_EXECUTABLE}" apply "${_patch}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE _result
    ERROR_VARIABLE _error)
if(NOT _result EQUAL 0)
    # Accept reapplication only when every hunk is already present.
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
            "GIT_CEILING_DIRECTORIES=${_source_parent}"
            "${GIT_EXECUTABLE}" apply --reverse --check "${_patch}"
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE _result
        ERROR_VARIABLE _reverse_error)
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR
            "Cannot apply ${PATCH_NAME}: unexpected content\n${_error}${_reverse_error}")
    endif()
endif()
