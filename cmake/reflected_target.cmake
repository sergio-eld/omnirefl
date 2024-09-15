# list(JOIN <list> <glue> <output variable>) 
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

function(reflected_target TARGET_NAME)
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
    list(JOIN EXCLUDED_LIST "," EXCLUDED)

    set(GENERATED_FILE "${CMAKE_CURRENT_BINARY_DIR}/reflected_${TARGET_NAME}.cpp")

    add_custom_command(OUTPUT ${GENERATED_FILE}
        # todo: `--resourse-dir` should be optional
        COMMAND omni::tool "--resource-dir=${TOOL_RESOURCE_DIR}"
            "-p=${CMAKE_BINARY_DIR}/"
            "-o=${GENERATED_FILE}"
            "--excluded=${EXCLUDED}"
            "${REFL_TARGET_SOURCES}"
        DEPENDS ${REFL_TARGET_SOURCES}
        COMMENT "Running ${TOOL_NAME} for ${TARGET_NAME}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        VERBATIM)

    add_custom_target(reflected_${TARGET_NAME} DEPENDS ${GENERATED_FILE})
    add_dependencies(${TARGET_NAME} reflected_${TARGET_NAME})

    target_sources(${TARGET_NAME} PRIVATE ${GENERATED_FILE})
    target_link_libraries(${TARGET_NAME} PUBLIC omni::refl)
endfunction() 
