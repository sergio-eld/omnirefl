cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

get_filename_component(_repository "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
file(READ "${_repository}/CMakeLists.txt" _project)
string(REGEX MATCH "project\\(omnirefl[^)]*VERSION ([0-9]+\\.[0-9]+\\.[0-9]+)"
    _version "${_project}")
set(_version "${CMAKE_MATCH_1}")

if(NOT OMNIREFL_PACKAGE_COMMIT)
    find_package(Git REQUIRED)
    # Bind mounts retain the host owner's UID inside packaging containers.
    execute_process(COMMAND "${GIT_EXECUTABLE}" -c "safe.directory=${_repository}"
        rev-parse --short=12 HEAD
        WORKING_DIRECTORY "${_repository}"
        OUTPUT_VARIABLE OMNIREFL_PACKAGE_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _result)
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR "Cannot determine the extras package commit")
    endif()
endif()
if(NOT OUTPUT_DIR)
    set(OUTPUT_DIR "${_repository}/artifacts/packages/extras")
endif()
get_filename_component(OUTPUT_DIR "${OUTPUT_DIR}" ABSOLUTE)

set(_name "omnirefl-extras-${_version}-${OMNIREFL_PACKAGE_COMMIT}")
set(_stage "${OUTPUT_DIR}/staging")
file(REMOVE_RECURSE "${_stage}")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/" DESTINATION "${_stage}/${_name}"
    PATTERN "internal" EXCLUDE
    PATTERN "scratch" EXCLUDE
    PATTERN "build*" EXCLUDE
    PATTERN "CMakeUserPresets.json" EXCLUDE
    PATTERN "package.cmake" EXCLUDE)
file(COPY "${_repository}/LICENSE" DESTINATION "${_stage}/${_name}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar czf
    "${OUTPUT_DIR}/${_name}.tar.gz" --format=gnutar "${_name}"
    WORKING_DIRECTORY "${_stage}" RESULT_VARIABLE _result)
if(NOT _result EQUAL 0)
    message(FATAL_ERROR "Cannot create extras archive")
endif()
file(REMOVE_RECURSE "${_stage}")
message(STATUS "Created ${OUTPUT_DIR}/${_name}.tar.gz")
