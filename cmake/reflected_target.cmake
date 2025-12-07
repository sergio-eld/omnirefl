cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

if(NOT OMNIREFL_FORCE_EXPORT_COMPILE_COMMANDS STREQUAL "OFF"
        AND NOT CMAKE_EXPORT_COMPILE_COMMANDS)
    message(STATUS "omnirefl: Forcing CMAKE_EXPORT_COMPILE_COMMANDS=ON")
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON
        CACHE BOOL "Export compile commands" FORCE)
endif()

# -- Reflection tool sanity checks --------
macro(_omni_checkhealth)
    if(NOT TARGET omni::tool)
        message(FATAL_ERROR
            "omnirefl: omni::tool target not found. Use find_package(omnirefl).")
    endif()

    if(NOT omnirefl_RESOURCE_DIR)
        message(FATAL_ERROR "omnirefl: omnirefl_RESOURCE_DIR is not set.")
    endif()

    if(NOT EXISTS "${omnirefl_RESOURCE_DIR}")
        message(FATAL_ERROR
            "omnirefl: resource dir \"${omnirefl_RESOURCE_DIR}\" not found.")
    endif()
endmacro()

# -- Target sources helper --------
function(_omni_get_target_sources target_name out_sources)
    if(NOT TARGET ${target_name})
        message(SEND_ERROR "${target_name} is not a valid CMake target")
        return()
    endif()

    get_target_property(target_sources ${target_name} SOURCES)
    if(NOT target_sources)
        set(${out_sources} "" PARENT_SCOPE)
        return()
    endif()

    set(abs_paths)
    foreach(src IN LISTS target_sources)
        get_filename_component(abs_path "${src}" ABSOLUTE)
        list(APPEND abs_paths "${abs_path}")
    endforeach()

    set(${out_sources} ${abs_paths} PARENT_SCOPE)
endfunction()

# -- Reflected target helper (default: source mode) --------
function(omni_reflected_target target)
    _omni_checkhealth()

    if(NOT TARGET ${target})
        message(SEND_ERROR "${target} is not a valid CMake target")
        return()
    endif()

    get_target_property(already_reflected ${target} _OMNIREFL_PROCESSED)
    if(already_reflected)
        message(SEND_ERROR
            "omnirefl: omni_reflected_target called multiple times for target ${target}.")
        return()
    endif()

    if(target MATCHES "^omni::(refl|tool)$")
        message(SEND_ERROR "omnirefl: cannot run reflection on ${target}")
        return()
    endif()

    # OBJECT libraries are not supported (would cause multiple definition
    # of explicit _call_impl specializations when their objects are reused).
    get_target_property(target_type ${target} TYPE)
    if(target_type STREQUAL "OBJECT_LIBRARY")
        message(SEND_ERROR
            "omnirefl: target ${target} is an OBJECT library, which is not supported. "
            "Use a STATIC library or an EXECUTABLE instead.")
        return()
    endif()

    set_target_properties(${target} PROPERTIES _OMNIREFL_PROCESSED TRUE)

    _omni_get_target_sources(${target} _all_sources)

    # fixme: path/to/src.cpp:path/to/output/from/compile_db
    set(_refl_sources)
    foreach(src IN LISTS _all_sources)
        if(src MATCHES "\\.(c|cc|cpp|cxx|h|hh|hpp|hxx)$")
            list(APPEND _refl_sources "${src}")
        endif()
    endforeach()

    # todo: do I actually care?
    if(NOT _refl_sources)
        message(SEND_ERROR
            "omnirefl: no C/C++ source or header files found for target ${target}")
        return()
    endif()

    set(_out_dir "${CMAKE_CURRENT_BINARY_DIR}/omni_${target}")
    set(_generated "${_out_dir}/reflected_${target}_source_mode.cpp")
    set(_comp_db "${CMAKE_BINARY_DIR}/compile_commands.json")

    message(STATUS "omnirefl: reflected target `${target}`")
    message(STATUS "omnirefl: selected source-mode for target ${target}")
    message(STATUS "omnirefl: output file for target ${target}: ${_generated}")

    # quote sources only for the command-line
    set(_refl_sources_quoted)
    foreach(src IN LISTS _refl_sources)
        list(APPEND _refl_sources_quoted "\"${src}\"")
    endforeach()

    set(_omni_args
        --mode=source
        --resource-dir "${omnirefl_RESOURCE_DIR}"
        --comp-db "${_comp_db}"
        -o "${_generated}"
        -s ${_refl_sources_quoted}
    )

    add_custom_command(OUTPUT "${_generated}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_out_dir}"
        COMMAND omni::tool ${_omni_args}
        COMMENT "Running omnirefl (source mode) for ${target}"
        DEPENDS ${_refl_sources}
        VERBATIM)

    # manual regeneration target: always reruns the tool when built.
    add_custom_target(${target}.omni
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_out_dir}"
        COMMAND omni::tool ${_omni_args}
        COMMENT "Running omnirefl (source mode) for ${target} [.omni]"
        VERBATIM)

    # fixme: remove this line, since it runs .omni unconditionally.
    # but for now it is simpler for me to debug on header changes 
    add_dependencies(${target} ${target}.omni)
    target_sources(${target} PRIVATE "${_generated}")
    target_link_libraries(${target} PRIVATE omni::refl)
endfunction()

