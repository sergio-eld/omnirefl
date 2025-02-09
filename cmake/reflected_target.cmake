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
    # Parse optional arguments
    set(OPTIONS PRINT_DEBUG)
    set(ONE_VALUE_ARGS)
    set(MULTI_VALUE_ARGS)
    cmake_parse_arguments(ARG 
        "${OPTIONS}" 
        "${ONE_VALUE_ARGS}"
        "${MULTI_VALUE_ARGS}" 
        ${ARGN})

    if (NOT ARG_PRINT_DEBUG)
        set(ARG_PRINT_DEBUG "")
    else()
        set(ARG_PRINT_DEBUG "--print-debug")
        message(STATUS "omnirefl: debug enabled for target ${TARGET_NAME}")
    endif()

    # todo: prevent calling twice on a single target
    # todo: prevent calling on itself
    set(TOOL_NAME omnirefl)
    if (NOT TARGET ${TARGET_NAME})
        message(SEND_ERROR "${TARGET_NAME} is not a valid CMake target")
    endif()
    if (NOT TARGET omni::tool)
        message(SEND_ERROR "Reflection instrument has not been detected. Use `find_package(${TOOL_NAME})`.")
    endif()
    # todo: check for target omni::refl
    # todo:
    # - check that compile commands generation option is enabled
    # - get the folder where `compile_commands.json` is generated to
    # - get all the sources (? including sources of linked static targets) 
    #  of the target and pass then to the program
    get_target_sources(${TARGET_NAME} REFL_TARGET_SOURCES)
    message(STATUS "${TOOL_NAME}: in target `${TARGET_NAME}` reflected sources:")
    foreach(_SRC ${REFL_TARGET_SOURCES})
        message(STATUS "${_SRC}")
    endforeach()

    set(TOOL_RESOURCE_DIR "${${TOOL_NAME}_RESOURCE_DIR}")
    if (NOT EXISTS ${TOOL_RESOURCE_DIR})
        message(FATAL_ERROR "${TOOL_NAME}: resource dir \"${TOOL_RESOURCE_DIR}\" for bundled headers not found")
    endif()

    set(EXCLUDED_LIST)
    # this will fail for old cmake versions
    # get_target_sources(omni::refl IGNORED_SELF_SOURCES)
    # list(APPEND EXCLUDED_LIST ${IGNORED_SELF_SOURCES})
    list(APPEND EXCLUDED_LIST ${CMAKE_BINARY_DIR})
    list(APPEND EXCLUDED_LIST ${GENERATED_FILE})
    list(JOIN EXCLUDED_LIST "," ARG_EXCLUDED)

    set(GENERATED_FILE "${CMAKE_CURRENT_BINARY_DIR}/reflected_${TARGET_NAME}.cpp")
    set(MARKER_FILE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.omni.marker")

    # TODO: Use target's binary dir instead of CMAKE_BINARY_DIR
    # Save tool arguments for reuse
    set(${TARGET_NAME}_OMNI_ARGS
        "${ARG_PRINT_DEBUG}"
        "--resource-dir=${TOOL_RESOURCE_DIR}"
        "-p=${CMAKE_BINARY_DIR}/"
        "-o=${GENERATED_FILE}"
        "--excluded=${CMAKE_BINARY_DIR},${GENERATED_FILE}"
        ${REFL_TARGET_SOURCES}
    )

    # manually regenerate
    add_custom_target(${TARGET_NAME}.omni
        COMMAND omni::tool ${${TARGET_NAME}_OMNI_ARGS}
        COMMENT "Running ${TOOL_NAME} for ${TARGET_NAME}"
        VERBATIM)

    # todo: (?) should this rerun if generated file has been updated
    # I have no idea how to implement it though...
    add_custom_command(OUTPUT ${GENERATED_FILE}
        COMMAND omni::tool ${${TARGET_NAME}_OMNI_ARGS}
        COMMENT "Running ${TOOL_NAME} for ${TARGET_NAME}"
        DEPENDS ${REFL_TARGET_SOURCES}
        VERBATIM)

    target_sources(${TARGET_NAME} PRIVATE ${GENERATED_FILE})
    target_link_libraries(${TARGET_NAME} PRIVATE omni::refl)
endfunction() 
