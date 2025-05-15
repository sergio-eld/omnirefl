# list(JOIN <list> <glue> <output variable>) since 3.12
cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

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
    foreach(_src ${TARGET_SOURCES})
        get_filename_component(ABS_PATH "${_src}" ABSOLUTE)
        list(APPEND ABS_PATHS "${ABS_PATH}")
    endforeach()
    set(${OUT_SOURCES} ${ABS_PATHS} PARENT_SCOPE)
endfunction()

# todo: add args
# - documentation:
#   - mention that omni::refl is linked privately
# - list of cpp files (to run only on the subset)
function(reflected_target target)
    set(tool_name omnirefl)

    if (NOT TARGET omni::tool)
        message(SEND_ERROR "Reflection instrument has not been detected."
            " Use `find_package(${tool_name})`.")
        return()
    endif()

    if (NOT TARGET ${target})
        message(SEND_ERROR "${target} is not a valid CMake target")
    endif()

    # Prevent calling twice on a single target
    get_target_property(already_reflected ${target} _OMNIREFL_PROCESSED)
    if (already_reflected)
        message(SEND_ERROR 
            "`reflected_target` called multiple times for target ${target}.")
        return()
    endif()
    set_target_properties(${target} PROPERTIES _OMNIREFL_PROCESSED TRUE)

    # Prevent calling on omni::refl or omni::tool
    if (target MATCHES "^omni::(refl|tool)$")
        message(SEND_ERROR "Cannot run reflection on ${target}")
        return()
    endif()

    set(OPTIONS 
        PRINT_DEBUG
        PRINT_INFO
        HEADER_MODE)
    set(ONE_VALUE_ARGS OUTPUT_DIR)
    set(MULTI_VALUE_ARGS)
    cmake_parse_arguments(ARG 
        "${OPTIONS}" 
        "${ONE_VALUE_ARGS}"
        "${MULTI_VALUE_ARGS}" 
        ${ARGN})

    if (ARG_PRINT_DEBUG)
        set(ARG_PRINT_DEBUG "--debug")
        message(STATUS "${tool_name}: debug output enabled for target ${target}")
    else()
        set(ARG_PRINT_DEBUG "")
    endif()

    # refactorme: pretty print block of text for reflected target
    message(STATUS "${tool_name}: reflected target `${target}`")
    if (ARG_HEADER_MODE)
        message(STATUS "${tool_name}: selected header-mode for target ${target}")
    else()
        message(STATUS "${tool_name}: selected source-mode for target ${target}")
    endif()

    if (ARG_PRINT_INFO)
        set(ARG_PRINT_INFO "--info")
        message(STATUS "${tool_name}: info output enabled for target ${target}")
    else()
        set(ARG_PRINT_INFO "")
    endif()

    if (ARG_EXCLUDE_SOURCES)
        set(ARG_EXCLUDE_SOURCES "--exclude")
        message(STATUS "${tool_name}: excluding specified sources for target ${target}")
    else()
        set(ARG_EXCLUDE_SOURCES "")
    endif()

    if (ARG_OUTPUT_DIR)
        # todo: validation
    else()
        set(ARG_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/omni_${target}")
    endif()

    message(STATUS "${tool_name}: output dir for target ${target}: ${ARG_OUTPUT_DIR}")

    get_target_sources(${target} refl_target_sources)

    message(STATUS "${tool_name}: in target `${target}` reflected sources:")
    foreach(_src ${refl_target_sources})
        message(STATUS "${_src}")
    endforeach()

    set(tool_resource_dir "${${tool_name}_RESOURCE_DIR}")
    if (NOT EXISTS ${tool_resource_dir})
        message(FATAL_ERROR "${tool_name}: resource dir \"${tool_resource_dir}\" for bundled headers not found")
    endif()

    if (ARG_HEADER_MODE)
        set(ARG_REFL_MODE "--header-mode")
        set(ARG_OUTPUT_FILE "")
    else()
        set(ARG_REFL_MODE "")
        set(ARG_OUTPUT_FILE "--output-file=reflected_${target}.cpp")
        set(generated_file "${ARG_OUTPUT_DIR}/reflected_${target}.cpp")
    endif()

    set(${target}_omni_args
        "${ARG_PRINT_DEBUG}"
        "${ARG_PRINT_INFO}"
        "${ARG_REFL_MODE}"
        "--resource-dir=${tool_resource_dir}"
        # todo: ensure that compilation db is there
        "--compilation-db=${CMAKE_BINARY_DIR}/"
        "--output-dir=${ARG_OUTPUT_DIR}"
        "${ARG_OUTPUT_FILE}"
        ${refl_target_sources}
    )

    # manually regenerate
    add_custom_target(${target}.omni
        COMMAND omni::tool ${${target}_omni_args}
        COMMENT "Running ${tool_name} for ${target}"
        VERBATIM)

    if (ARG_HEADER_MODE)
        set(generated_headers)
        foreach(_src ${refl_target_sources})
            get_filename_component(file_name "${_src}" NAME_WE)
            set(generated_header "${ARG_OUTPUT_DIR}/${file_name}.hpp")
            list(APPEND generated_headers ${generated_header})

            # todo: rerun the tool only on modified .cpp files:
            # for example, if 2/10 files were modified, only 2 must be rerun.

            # todo: check that non-reflected sources do not trigger rerun
            # Ensure generated headers are tracked as build outputs
            set_source_files_properties(${_src} PROPERTIES
                OBJECT_DEPENDS "${generated_header}")

            if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
                set(force_include_flag "/FI" "${generated_header}")
            else()
                set(force_include_flag "-include" "${generated_header}")
            endif()
            message(STATUS "${tool_name}: For target ${target} "
                "forcing include '${force_include_flag}'")

            get_source_file_property(current_flags ${_src} COMPILE_OPTIONS)
            if (NOT current_flags)
                set(current_flags "")
            endif()
            list(APPEND current_flags ${force_include_flag})
            set(force_include_property "${file_name}_FORCE_INCLUDE")
            set_target_properties(${target} PROPERTIES ${force_include_property} "${current_flags}")
            # fixme:
            #   this will cause conflicts if other targets use this source file for compilation 
            #   (with different flags, options, etc)
            set_source_files_properties(${_src} 
                PROPERTIES 
                COMPILE_OPTIONS $<TARGET_PROPERTY:${force_include_property}>
            )
        endforeach()

        target_compile_definitions(${target} PRIVATE OMNI_HEADER_REFLECTION)

        add_custom_command(OUTPUT ${generated_headers}
            COMMAND omni::tool ${${target}_omni_args}
            COMMENT "Running ${tool_name} for ${target}"
            DEPENDS ${refl_target_sources}
            VERBATIM)
    else()
        # todo: (?) should this rerun if generated file has been updated
        # I have no idea how to implement it though...
        add_custom_command(OUTPUT ${generated_file}
            COMMAND omni::tool ${${target}_omni_args}
            COMMENT "Running ${tool_name} for ${target}"
            DEPENDS ${refl_target_sources}
            VERBATIM)
        target_sources(${target} PRIVATE ${generated_file})
    endif()

    target_link_libraries(${target} PRIVATE omni::refl)
endfunction() 
