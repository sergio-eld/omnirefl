cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

# Ninja-only: controls how CMake rewrites DEPFILE paths for Ninja rules
if(CMAKE_GENERATOR MATCHES "Ninja" AND POLICY CMP0116)
    cmake_policy(SET CMP0116 NEW)
    message(VERBOSE "omnirefl: CMP0116=NEW (Ninja DEPFILE)")
endif()

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
    get_target_property(srcs "${target_name}" SOURCES)
    if(srcs STREQUAL "srcs-NOTFOUND")
        set(${out_sources} "" PARENT_SCOPE)
        return()
    endif()

    set(result)
    foreach(s IN LISTS srcs)
        # Keep genex entries (cannot absolutize/filter at configure time)
        if(s MATCHES "\\$<")
            list(APPEND result "${s}")
            continue()
        endif()

        get_filename_component(abs "${s}" ABSOLUTE)

        # Filter generated sources (only for non-genex)
        get_source_file_property(is_gen "${abs}" GENERATED)
        if(is_gen)
            continue()
        endif()

        list(APPEND result "${abs}")
    endforeach()

    set(${out_sources} "${result}" PARENT_SCOPE)
endfunction()

# todo: implement include|exclude handling
# -- Reflected target helper (default: source mode) --------
# args: MODE source|header (default: source), INCLUDE <...> | EXCLUDE <...>
# Generated sources are ignored by the tool.
# Use INCLUDE or EXCLUDE to refine inputs; entries are file paths or regexes.
# INCLUDE and EXCLUDE are mutually exclusive.
function(omni_reflected_target target)
    _omni_checkhealth()

    cmake_parse_arguments(OMNIREFL "" "MODE" "INCLUDE;EXCLUDE" ${ARGN})

    if(OMNIREFL_INCLUDE AND OMNIREFL_EXCLUDE)
        message(FATAL_ERROR "omni_reflected_target: INCLUDE and EXCLUDE are mutually exclusive")
    endif()

    if(NOT OMNIREFL_MODE)
        set(OMNIREFL_MODE "source")
    endif()
    set(mode "${OMNIREFL_MODE}")
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

    # OBJECT and INTERFACE libraries are not supported.
    # - OBJECT: can cause multiple definition of explicit _call_impl specializations when reused.
    # - INTERFACE: has no compilation step / sources.
    get_target_property(target_type ${target} TYPE)
    if(target_type STREQUAL "OBJECT_LIBRARY")
        message(SEND_ERROR
            "omnirefl: target ${target} is an OBJECT library, which is not supported. "
            "Use a STATIC library or an EXECUTABLE instead.")
        return()
    endif()
    if(target_type STREQUAL "INTERFACE_LIBRARY")
        message(SEND_ERROR
            "omnirefl: target ${target} is an INTERFACE library, which is not supported. "
            "Use a STATIC library or an EXECUTABLE instead.")
        return()
    endif()

    set_target_properties(${target} PROPERTIES _OMNIREFL_PROCESSED TRUE)

    _omni_get_target_sources(${target} _all_sources)

    set(_refl_sources)
    foreach(src IN LISTS _all_sources)
        if(src MATCHES "\\.(c|cc|cpp|cxx|h|hh|hpp|hxx)$")
            list(APPEND _refl_sources "${src}")
        endif()
    endforeach()

    if(NOT _refl_sources)
        message(SEND_ERROR
            "omnirefl: no C/C++ source or header files found for target ${target}")
        return()
    endif()

    set(_out_dir "${CMAKE_CURRENT_BINARY_DIR}/omni_${target}")
    set(_comp_db "${CMAKE_BINARY_DIR}/compile_commands.json")

    message(STATUS "omnirefl: reflected target `${target}`")

    # Common: require at least one TU source (headers-only targets not supported)
    set(_tu_sources)
    foreach(src IN LISTS _refl_sources)
        if(src MATCHES "\\.(c|cc|cpp|cxx)$")
            list(APPEND _tu_sources "${src}")
        endif()
    endforeach()
    if(NOT _tu_sources)
        message(SEND_ERROR
            "omnirefl: target ${target} has no C/C++ source files (.c/.cc/.cpp/.cxx). "
            "Headers-only targets are not supported.")
        return()
    endif()

    # Shared lists for common plumbing
    set(_gen_outputs)
    set(_gen_depfiles)

    if(mode STREQUAL "source")
        set(_generated "${_out_dir}/reflected_${target}_source_mode.cpp")
        set(_depfile "${_generated}.d")

        message(STATUS "omnirefl: selected source-mode for target ${target}")
        message(STATUS "omnirefl: output file for target ${target}: ${_generated}")
        message(STATUS "omnirefl: depfile for target ${target}: ${_depfile}")

        # Build all "src":"target.dir" pairs (headers included)
        set(_pairs_quoted)
        foreach(src IN LISTS _refl_sources)
            set(_pair "\"${src}\":\"${target}.dir\"")
            list(APPEND _pairs_quoted ${_pair})
            message(VERBOSE "omnirefl: for target ${target} selected source: ${_pair}")
        endforeach()

        set(_omni_args
            --mode=source
            --resource-dir "${omnirefl_RESOURCE_DIR}"
            --comp-db "${_comp_db}"
            -o "${_generated}"
            -s ${_pairs_quoted}
        )

        add_custom_command(
            OUTPUT "${_generated}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_out_dir}"
            COMMAND omni::tool ${_omni_args}
            COMMENT "omnirefl: running (source mode) for ${target}"
            DEPENDS ${_refl_sources} omni::tool
            BYPRODUCTS "${_depfile}"
            DEPFILE "${_depfile}"
            VERBATIM
        )

        list(APPEND _gen_outputs "${_generated}")
        list(APPEND _gen_depfiles "${_depfile}")

        target_sources(${target} PRIVATE "${_generated}")
        target_link_libraries(${target} PRIVATE omni::refl)
        target_compile_definitions(${target} PRIVATE OMNI_SOURCE_MODE)

    else() # header
        message(STATUS "omnirefl: selected header-mode for target ${target}")
        message(STATUS "omnirefl: output dir for target ${target}: ${_out_dir}")

        foreach(_src IN LISTS _tu_sources)
            get_filename_component(_abs "${_src}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
            string(SHA1 _src_hash "${_abs}")
            get_filename_component(_stem "${_src}" NAME_WE)

            set(_generated_header "${_out_dir}/${_stem}_${_src_hash}.omnirefl.hpp")
            set(_depfile "${_generated_header}.d")

            set(_pair "\"${_src}\":\"${target}.dir\"")
            message(VERBOSE "omnirefl: for target ${target} selected TU: ${_pair}")

            set(_omni_args
                --mode=header
                --resource-dir "${omnirefl_RESOURCE_DIR}"
                --comp-db "${_comp_db}"
                -o "${_generated_header}"
                -s ${_pair}
            )

            add_custom_command(
                OUTPUT "${_generated_header}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${_out_dir}"
                COMMAND omni::tool ${_omni_args}
                COMMENT "omnirefl: running (header mode) for ${target}: ${_src}"
                DEPENDS "${_src}" omni::tool
                BYPRODUCTS "${_depfile}"
                DEPFILE "${_depfile}"
                VERBATIM
            )

            list(APPEND _gen_outputs "${_generated_header}")
            list(APPEND _gen_depfiles "${_depfile}")

            set(_fi_prop "OMNIREFL_FORCE_INCLUDE_${_src_hash}")

            if(MSVC)
                set_target_properties(${target} PROPERTIES ${_fi_prop} "/FI${_generated_header}")
            else()
                set_target_properties(${target} PROPERTIES ${_fi_prop} "-include;${_generated_header}")
            endif()

            set_source_files_properties("${_src}" PROPERTIES COMPILE_OPTIONS "$<TARGET_PROPERTY:${_fi_prop}>")
        endforeach()

        target_compile_definitions(${target} PRIVATE OMNI_HEADER_MODE)
        target_link_libraries(${target} PRIVATE omni::refl)
    endif()

    # Common plumbing: hook outputs into build + manual force-regenerate target
    set(_gen_target "_gen.${target}.omni")
    add_custom_target(${_gen_target} DEPENDS ${_gen_outputs})
    add_dependencies(${target} ${_gen_target})

    set(_cfg_arg "")
    if(CMAKE_CONFIGURATION_TYPES)
        set(_cfg_arg --config $<CONFIG>)
    endif()

    add_custom_target(${target}.omni
        COMMAND ${CMAKE_COMMAND} -E rm -f ${_gen_outputs} ${_gen_depfiles}
        COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" --target ${_gen_target} ${_cfg_arg}
        COMMENT "omnirefl: force-regenerate for ${target} [.omni]"
        VERBATIM
    )
endfunction()

