cmake_minimum_required(VERSION 3.19 FATAL_ERROR)

# WARNING: Public FetchContent entry point. Keep cmake/integration/CMakeLists.txt
# at this path: consumers use SOURCE_SUBDIR cmake/integration, and moving or
# renaming it breaks their existing declarations. Repository-relative paths
# below also depend on this directory's position beneath the repository root.

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

include(FetchContent)

# FIXME: [external] FetchContent has no public API for the requested GIT_TAG.
# Keep the declaration name `omnirefl`; inspecting checkout tags cannot tell a
# release request from an explicit commit request pointing at the same revision.
# Replace the internal property lookup when CMake exposes declaration details.
get_property(_details GLOBAL PROPERTY _FetchContent_omnirefl_savedDetails)
cmake_parse_arguments(_request "" "GIT_TAG" "" ${_details})
if(NOT _request_GIT_TAG)
    message(FATAL_ERROR "Declare omnirefl with GIT_TAG and SOURCE_SUBDIR cmake/integration")
endif()

get_filename_component(_checkout "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
if(_request_GIT_TAG MATCHES "^v([0-9]+\\.[0-9]+\\.[0-9]+)$")
    string(REPLACE "." "\\." _version_pattern "${CMAKE_MATCH_1}")
    # Release asset filenames include a commit suffix. Query the selected tag
    # after publication rather than embedding its not-yet-known URL/checksum.
    set(_metadata "${CMAKE_CURRENT_BINARY_DIR}/release-${_request_GIT_TAG}.json")
    if(NOT EXISTS "${_metadata}")
        file(DOWNLOAD
            "https://api.github.com/repos/sergio-eld/omnirefl/releases/tags/${_request_GIT_TAG}"
            "${_metadata}.tmp" TLS_VERIFY ON STATUS _download_status)
        list(GET _download_status 0 _download_error)
        if(_download_error)
            message(FATAL_ERROR "Cannot read omnirefl release ${_request_GIT_TAG}: ${_download_status}")
        endif()
        file(RENAME "${_metadata}.tmp" "${_metadata}")
    endif()
    file(READ "${_metadata}" _release)
    string(JSON _count LENGTH "${_release}" assets)
    set(_url)
    set(_digest)
    if(_count GREATER 0)
        math(EXPR _last "${_count} - 1")
        foreach(_index RANGE 0 ${_last})
            string(JSON _name GET "${_release}" assets ${_index} name)
            if(_name MATCHES "^omnirefl-cosmo-universal-${_version_pattern}-[0-9a-f]+\\.tar\\.gz$")
                if(_url)
                    message(FATAL_ERROR "Expected one universal archive in ${_request_GIT_TAG}")
                endif()
                string(JSON _url GET "${_release}" assets ${_index} browser_download_url)
                string(JSON _digest ERROR_VARIABLE _digest_error
                    GET "${_release}" assets ${_index} digest)
            endif()
        endforeach()
    endif()
    if(NOT _url OR NOT _digest MATCHES "^sha256:[0-9a-f]+$")
        message(FATAL_ERROR "Expected a universal archive with a SHA-256 digest in ${_request_GIT_TAG}")
    endif()
    string(REPLACE "sha256:" "SHA256=" _hash "${_digest}")
    FetchContent_Declare(omnirefl_distribution
        URL "${_url}"
        URL_HASH "${_hash}"
        TLS_VERIFY ON)
    FetchContent_MakeAvailable(omnirefl_distribution)
    set(_prefix "${omnirefl_distribution_SOURCE_DIR}")
else()
    string(LENGTH "${_request_GIT_TAG}" _length)
    if(NOT _length EQUAL 40 OR NOT _request_GIT_TAG MATCHES "^[0-9a-fA-F]+$")
        message(FATAL_ERROR "omnirefl GIT_TAG must be a release tag (vX.Y.Z) or full commit hash")
    endif()
    if(NOT CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
        message(FATAL_ERROR "omnirefl source builds currently require an x86_64 host")
    endif()
    find_program(_docker docker REQUIRED)
    find_package(Git REQUIRED)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
        WORKING_DIRECTORY "${_checkout}" OUTPUT_VARIABLE _commit
        OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
    string(TOLOWER "${_request_GIT_TAG}" _requested_commit)
    if(NOT _commit STREQUAL _requested_commit)
        message(FATAL_ERROR "omnirefl checkout does not match requested commit ${_request_GIT_TAG}")
    endif()
    set(_work "${CMAKE_CURRENT_BINARY_DIR}/${_commit}")
    set(_prefix "${_work}/distribution")
    if(NOT EXISTS "${_prefix}/lib/cmake/omnirefl/omnireflConfig.cmake")
        file(MAKE_DIRECTORY "${_work}/build" "${_work}/packages")
        set(_user)
        if(CMAKE_HOST_UNIX)
            execute_process(COMMAND id -u OUTPUT_VARIABLE _uid
                OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
            execute_process(COMMAND id -g OUTPUT_VARIABLE _gid
                OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
            set(_user --user "${_uid}:${_gid}")
        endif()
        string(SUBSTRING "${_commit}" 0 12 _short_commit)
        set(_environment "${CMAKE_COMMAND}" -E env
            "PACKAGE_DIR=${_work}/packages"
            "PACKAGE_COMMIT=${_short_commit}" "COSMO_BUILD_UNIVERSAL=true")
        execute_process(COMMAND ${_environment} "${_docker}" compose
            --file docker-compose.yml pull build-cosmo
            WORKING_DIRECTORY "${_checkout}" COMMAND_ERROR_IS_FATAL ANY)
        execute_process(COMMAND ${_environment} "${_docker}" compose
            --file docker-compose.yml run --rm --no-deps ${_user}
            --volume "${_work}/build:/build" build-cosmo
            WORKING_DIRECTORY "${_checkout}" COMMAND_ERROR_IS_FATAL ANY)
        file(GLOB _archives "${_work}/packages/omnirefl-cosmo-universal-*.tar.gz")
        list(LENGTH _archives _count)
        if(NOT _count EQUAL 1)
            message(FATAL_ERROR "Expected one universal archive from build-cosmo")
        endif()
        file(ARCHIVE_EXTRACT INPUT "${_archives}" DESTINATION "${_work}/unpacked")
        file(GLOB _roots LIST_DIRECTORIES TRUE "${_work}/unpacked/*")
        list(LENGTH _roots _count)
        if(NOT _count EQUAL 1)
            message(FATAL_ERROR "Expected one installation directory in omnirefl archive")
        endif()
        file(RENAME "${_roots}" "${_prefix}")
    endif()
endif()

set(_config "${_prefix}/lib/cmake/omnirefl")
set(omnirefl_ROOT "${_prefix}" PARENT_SCOPE)
if(CMAKE_FIND_PACKAGE_REDIRECTS_DIR)
    # FetchContent otherwise writes an empty config that masks the native one.
    # Forward components and version checks through the original package files.
    file(CONFIGURE OUTPUT "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/omnirefl-config.cmake"
        CONTENT "include(\"@_config@/omnireflConfig.cmake\")\n" @ONLY)
    file(CONFIGURE OUTPUT "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/omnirefl-config-version.cmake"
        CONTENT "include(\"@_config@/omnireflConfigVersion.cmake\")\n" @ONLY)
endif()
