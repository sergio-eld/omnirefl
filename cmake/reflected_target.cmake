# list(JOIN <list> <glue> <output variable>) since 3.12
cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

if (NOT OMNIREFL_FORCE_EXPORT_COMPILE_COMMANDS STREQUAL "OFF")
    message(STATUS "omnirefl: Forcing `CMAKE_EXPORT_COMPILE_COMMNADS=1")
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile commands" FORCE)
endif()

function(get_target_sources TARGET_NAME OUT_SOURCES)
    if (NOT TARGET ${TARGET_NAME})
        message(SEND_ERROR "${TARGET_NAME} is not a valid CMake Target")
    endif()
    get_target_property(TARGET_SOURCES ${TARGET_NAME} SOURCES)
    if (NOT TARGET_SOURCES)
        return()
    endif()

    set(ABS_PATHS)
    foreach(_SRC ${TARGET_SOURCES})
        get_filename_component(ABS_PATH "${_SRC}" ABSOLUTE)
        list(APPEND ABS_PATHS "${ABS_PATH}")
    endforeach()
    set(${OUT_SOURCES} ${ABS_PATHS} PARENT_SCOPE)
endfunction()

# todo: add args
# - documentation:
#   - mention that omni::refl is linked privately
# - list of cpp files (to run only on the subset)
# - output args string to run omnirefl tool with
function(reflected_target TARGET_NAME)
    set(TOOL_NAME omnirefl)

    if (NOT TARGET omni::tool)
        message(SEND_ERROR "Reflection instrument has not been detected."
            " Use `find_package(${TOOL_NAME})`.")
        return()
    endif()

    if (NOT TARGET ${TARGET_NAME})
        message(SEND_ERROR "${TARGET_NAME} is not a valid CMake target")
    endif()

    # Prevent calling twice on a single target
    get_target_property(ALREADY_REFLECTED ${TARGET_NAME} _OMNIREFL_PROCESSED)
    if (ALREADY_REFLECTED)
        message(SEND_ERROR 
            "`reflected_target` called multiple times for target ${TARGET_NAME}.")
        return()
    endif()
    set_target_properties(${TARGET_NAME} PROPERTIES _OMNIREFL_PROCESSED TRUE)

    # Prevent calling on omni::refl or omni::tool
    if (TARGET_NAME MATCHES "^omni::(refl|tool)$")
        message(SEND_ERROR "Cannot run reflection on ${TARGET_NAME}")
        return()
    endif()

    set(OPTIONS 
        PRINT_DEBUG
        PRINT_INFO
        # todo: I am not sure this arg makes sense here
        # EXCLUDE_SOURCES
        INPLACE_MODE)
    set(ONE_VALUE_ARGS OUTPUT_DIR)
    set(MULTI_VALUE_ARGS SOURCES)
    cmake_parse_arguments(ARG 
        "${OPTIONS}" 
        "${ONE_VALUE_ARGS}"
        "${MULTI_VALUE_ARGS}" 
        ${ARGN})

    if (ARG_PRINT_DEBUG)
        set(ARG_PRINT_DEBUG "--debug")
        message(STATUS "${TOOL_NAME}: debug output enabled for target ${TARGET_NAME}")
    else()
        set(ARG_PRINT_DEBUG "")
    endif()

    message(STATUS "${TOOL_NAME}: reflected target `${TARGET_NAME}`")
    if (ARG_INPLACE_MODE)
        message(STATUS "${TOOL_NAME}: selected inplace-mode for target ${TARGET_NAME}")
    else()
        message(STATUS "${TOOL_NAME}: selected target-mode for target ${TARGET_NAME}")
    endif()

    if (ARG_PRINT_INFO)
        set(ARG_PRINT_INFO "--info")
        message(STATUS "${TOOL_NAME}: info output enabled for target ${TARGET_NAME}")
    else()
        set(ARG_PRINT_INFO "")
    endif()

    if (ARG_EXCLUDE_SOURCES)
        set(ARG_EXCLUDE_SOURCES "--exclude")
        message(STATUS "${TOOL_NAME}: excluding specified sources for target ${TARGET_NAME}")
    else()
        set(ARG_EXCLUDE_SOURCES "")
    endif()

    if (ARG_OUTPUT_DIR)
        # todo: validation
    else()
        set(ARG_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    endif()

    message(STATUS "${TOOL_NAME}: output dir for target ${TARGET_NAME}: ${ARG_OUTPUT_DIR}")

    get_target_sources(${TARGET_NAME} REFL_TARGET_SOURCES)
    if (ARG_SOURCES)
        # todo: for each source the full path is needed,
        # because for inplace-mode we need to force include
        # the generated reflected header: ${output_dir}/{file_name}.hpp
        # At this point we already have REFL_TARGET_SOURCES evaluated
    endif()

    message(STATUS "${TOOL_NAME}: in target `${TARGET_NAME}` reflected sources:")
    foreach(_SRC ${REFL_TARGET_SOURCES})
        message(STATUS "${_SRC}")
    endforeach()

    set(TOOL_RESOURCE_DIR "${${TOOL_NAME}_RESOURCE_DIR}")
    if (NOT EXISTS ${TOOL_RESOURCE_DIR})
        message(FATAL_ERROR "${TOOL_NAME}: resource dir \"${TOOL_RESOURCE_DIR}\" for bundled headers not found")
    endif()

    if (ARG_INPLACE_MODE)
        set(ARG_REFL_MODE "--inplace-mode")
        set(ARG_OUTPUT_FILE "")
    else()
        set(ARG_REFL_MODE "")
        set(ARG_OUTPUT_FILE "--output-file=reflected_${TARGET_NAME}.cpp")
        set(GENERATED_FILE "${CMAKE_CURRENT_BINARY_DIR}/reflected_${TARGET_NAME}.cpp")
    endif()

    set(${TARGET_NAME}_OMNI_ARGS
        "${ARG_PRINT_DEBUG}"
        "${ARG_PRINT_INFO}"
        "${ARG_REFL_MODE}"
        "--resource-dir=${TOOL_RESOURCE_DIR}"
        # todo: ensure that compilation db is there
        "--compilation-db=${CMAKE_BINARY_DIR}/"
        "--output-dir=${ARG_OUTPUT_DIR}"
        "${ARG_OUTPUT_FILE}"
        ${REFL_TARGET_SOURCES}
    )

    # manually regenerate
    add_custom_target(${TARGET_NAME}.omni
        COMMAND omni::tool ${${TARGET_NAME}_OMNI_ARGS}
        COMMENT "Running ${TOOL_NAME} for ${TARGET_NAME}"
        VERBATIM)

    if (ARG_INPLACE_MODE)
        set(GENERATED_HEADERS)
        foreach(_SRC ${REFL_TARGET_SOURCES})
            get_filename_component(FILE_NAME "${_SRC}" NAME_WE)
            set(GENERATED_HEADER "${ARG_OUTPUT_DIR}/${FILE_NAME}.hpp")
            list(APPEND GENERATED_HEADERS ${GENERATED_HEADER})

            # todo: rerun the tool only on modified .cpp files:
            # for example, if 2/10 files were modified, only 2 must be rerun.

            # todo: check that non-reflected sources do not trigger rerun
            # Ensure generated headers are tracked as build outputs
            set_source_files_properties(${_SRC} PROPERTIES
                OBJECT_DEPENDS "${GENERATED_HEADER}")

            if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
                set(FORCE_INCLUDE_FLAG "/FI\"${GENERATED_HEADER}\"")
            else()
                set(FORCE_INCLUDE_FLAG "-include" "${GENERATED_HEADER}")
            endif()
            message(STATUS "${TOOL_NAME}: For target ${TARGET_NAME} "
                "forcing include '${FORCE_INCLUDE_FLAG}'")

            get_source_file_property(CURRENT_FLAGS ${_SRC} COMPILE_OPTIONS)
            if (NOT CURRENT_FLAGS)
                set(CURRENT_FLAGS "")
            endif()
            list(APPEND CURRENT_FLAGS ${FORCE_INCLUDE_FLAG})
            set_source_files_properties(${_SRC} PROPERTIES
                COMPILE_OPTIONS "${CURRENT_FLAGS}")
        endforeach()

        target_compile_definitions(${TARGET_NAME} PRIVATE OMNI_INPLACE_REFLECTION)

        add_custom_command(OUTPUT ${GENERATED_HEADERS}
            COMMAND omni::tool ${${TARGET_NAME}_OMNI_ARGS}
            COMMENT "Running ${TOOL_NAME} for ${TARGET_NAME}"
            DEPENDS ${REFL_TARGET_SOURCES}
            VERBATIM)
    else()
        # todo: (?) should this rerun if generated file has been updated
        # I have no idea how to implement it though...
        add_custom_command(OUTPUT ${GENERATED_FILE}
            COMMAND omni::tool ${${TARGET_NAME}_OMNI_ARGS}
            COMMENT "Running ${TOOL_NAME} for ${TARGET_NAME}"
            DEPENDS ${REFL_TARGET_SOURCES}
            VERBATIM)
        target_sources(${TARGET_NAME} PRIVATE ${GENERATED_FILE})
    endif()

    target_link_libraries(${TARGET_NAME} PRIVATE omni::refl)
endfunction() 
