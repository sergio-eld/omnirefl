cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

# Public reflection headers use constexpr and require C++11 or newer.
# Reject older configured modes without changing the target's requirements.
function(_omni_check_minimum_cxx_standard target)
    get_target_property(_standard "${target}" CXX_STANDARD)
    if(NOT _standard)
        set(_standard "${CMAKE_CXX_STANDARD_COMPUTED_DEFAULT}")
    endif()

    # Standard numbers are not chronological: 98 is older than 11.
    if(_standard STREQUAL "98" OR (_standard AND _standard LESS 11))
        message(FATAL_ERROR "omnirefl: target '${target}' uses C++${_standard}; C++11 or newer is required")
    endif()
endfunction()

# Ninja-only: controls how CMake rewrites DEPFILE paths for Ninja rules
if(CMAKE_GENERATOR MATCHES "Ninja" AND POLICY CMP0116)
    cmake_policy(SET CMP0116 NEW)
    message(VERBOSE "omnirefl: CMP0116=NEW (Ninja DEPFILE)")
endif()

if(NOT OMNIREFL_FORCE_EXPORT_COMPILE_COMMANDS STREQUAL "OFF"
        AND NOT CMAKE_EXPORT_COMPILE_COMMANDS)
    message(WARNING
        "omnirefl: enabling CMAKE_EXPORT_COMPILE_COMMANDS, which is required "
        "for reflection. Configure with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "
        "or set it before creating targets. Targets created before "
        "find_package(omnirefl) may still have export disabled.")
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON
        CACHE BOOL "Export compile commands" FORCE)
endif()

# ad hoc: CMake 4 permits Apple compilers to select their default SDK
# implicitly, which leaves compile_commands.json without the SDK required by
# Omnirefl's embedded driver. Resolve that fallback lazily, once per configure,
# instead of launching discovery from every tool invocation.
# TODO: Move this policy into the planned compiler-environment discovery tool.
function(_omni_resolve_macos_sdk_fallback out)
    get_property(_sysroot GLOBAL PROPERTY _OMNIREFL_MACOS_SYSROOT)
    if(NOT _sysroot)
        if(DEFINED ENV{SDKROOT} AND IS_ABSOLUTE "$ENV{SDKROOT}")
            set(_sysroot "$ENV{SDKROOT}")
        else()
            set(_sdk "macosx")
            if(DEFINED ENV{SDKROOT} AND NOT "$ENV{SDKROOT}" STREQUAL "")
                set(_sdk "$ENV{SDKROOT}")
            endif()

            execute_process(
                COMMAND xcrun --sdk "${_sdk}" --show-sdk-path
                RESULT_VARIABLE _sdk_result
                OUTPUT_VARIABLE _sysroot
                ERROR_VARIABLE _sdk_error
                OUTPUT_STRIP_TRAILING_WHITESPACE)

            if(NOT 0 EQUAL _sdk_result OR NOT IS_ABSOLUTE "${_sysroot}")
                message(FATAL_ERROR
                    "omnirefl: CMake failed to resolve the macOS SDK '${_sdk}'. "
                    "Configure with CMAKE_OSX_SYSROOT set explicitly.\n"
                    "${_sdk_error}")
            endif()
        endif()

        set_property(GLOBAL PROPERTY _OMNIREFL_MACOS_SYSROOT "${_sysroot}")
    endif()

    set(${out} "${_sysroot}" PARENT_SCOPE)
endfunction()

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
function(_omni_classify_target_sources
        target_name
        out_cxx_sources
        out_c_sources
        out_generator_expressions)
    get_target_property(srcs "${target_name}" SOURCES)
    if(srcs STREQUAL "srcs-NOTFOUND")
        set(${out_cxx_sources} "" PARENT_SCOPE)
        set(${out_c_sources} "" PARENT_SCOPE)
        set(${out_generator_expressions} "" PARENT_SCOPE)
        return()
    endif()

    set(cxx_sources)
    set(c_sources)
    set(generator_expressions)
    get_target_property(target_source_dir "${target_name}" SOURCE_DIR)
    get_target_property(target_binary_dir "${target_name}" BINARY_DIR)

    foreach(s IN LISTS srcs)
        if(s MATCHES "\\$<")
            list(APPEND generator_expressions "${s}")
            continue()
        endif()

        get_filename_component(abs "${s}" ABSOLUTE
            BASE_DIR "${target_source_dir}")

        set(source_candidates "${abs}")
        if(NOT IS_ABSOLUTE "${s}")
            get_filename_component(generated_abs "${s}" ABSOLUTE
                BASE_DIR "${target_binary_dir}")
            list(APPEND source_candidates "${generated_abs}")
        endif()

        set(is_gen FALSE)
        foreach(candidate IN LISTS source_candidates)
            get_source_file_property(candidate_is_gen "${candidate}"
                TARGET_DIRECTORY "${target_name}" GENERATED)
            if(candidate_is_gen)
                set(is_gen TRUE)
                break()
            endif()
        endforeach()

        if(is_gen)
            # TODO(low): consider a per-source override flag to instrument
            # selected GENERATED translation units. Keep generated helpers
            # such as Qt moc output excluded by default.
            continue()
        endif()

        get_source_file_property(header_only "${abs}"
            TARGET_DIRECTORY "${target_name}" HEADER_FILE_ONLY)
        if(header_only)
            continue()
        endif()

        get_source_file_property(language "${abs}"
            TARGET_DIRECTORY "${target_name}" LANGUAGE)

        if("NOTFOUND" STREQUAL language)
            get_filename_component(extension "${s}" EXT)
            string(REGEX REPLACE "^\\." "" extension "${extension}")

            if(extension IN_LIST CMAKE_CXX_SOURCE_FILE_EXTENSIONS)
                set(language CXX)
            elseif(extension IN_LIST CMAKE_C_SOURCE_FILE_EXTENSIONS)
                set(language C)
            endif()
        endif()

        if("CXX" STREQUAL language)
            list(APPEND cxx_sources "${abs}")
        elseif("C" STREQUAL language)
            list(APPEND c_sources "${abs}")
        endif()
    endforeach()

    set(${out_cxx_sources} "${cxx_sources}" PARENT_SCOPE)
    set(${out_c_sources} "${c_sources}" PARENT_SCOPE)
    set(${out_generator_expressions} "${generator_expressions}" PARENT_SCOPE)
endfunction()

# -- Reflected target helper --------
# Generated sources are ignored by the tool.
function(omni_reflected_target target)
    _omni_checkhealth()

    cmake_parse_arguments(
        OMNIREFL
        "ENABLE_INDEX_MODE;NO_ANNOTATIONS;TIMINGS;_DISABLE_PREVIEW_AD_HOC"
        "LOG_LEVEL"
        ""
        ${ARGN})

    if(OMNIREFL_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "omni_reflected_target: unexpected arguments: ${OMNIREFL_UNPARSED_ARGUMENTS}")
    endif()

    if(NOT OMNIREFL_LOG_LEVEL)
        if(CMAKE_MESSAGE_LOG_LEVEL)
            string(TOUPPER "${CMAKE_MESSAGE_LOG_LEVEL}" _omni_cmake_log_level)
            if("ERROR" STREQUAL _omni_cmake_log_level)
                set(OMNIREFL_LOG_LEVEL "error")
            elseif("WARNING" STREQUAL _omni_cmake_log_level)
                set(OMNIREFL_LOG_LEVEL "warning")
            elseif("DEBUG" STREQUAL _omni_cmake_log_level
                    OR "TRACE" STREQUAL _omni_cmake_log_level)
                set(OMNIREFL_LOG_LEVEL "debug")
            elseif("NOTICE" STREQUAL _omni_cmake_log_level
                    OR "STATUS" STREQUAL _omni_cmake_log_level
                    OR "VERBOSE" STREQUAL _omni_cmake_log_level)
                set(OMNIREFL_LOG_LEVEL "info")
            else()
                set(OMNIREFL_LOG_LEVEL "info")
            endif()
        else()
            set(OMNIREFL_LOG_LEVEL "info")
        endif()
    endif()

    if(NOT OMNIREFL_LOG_LEVEL MATCHES "^(silent|error|warning|info|debug)$")
        message(FATAL_ERROR
            "omni_reflected_target: LOG_LEVEL must be one of "
            "silent, error, warning, info, debug")
    endif()

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
    # - OBJECT: can cause duplicated generated reflection metadata when reused.
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

    if("Darwin" STREQUAL CMAKE_SYSTEM_NAME)
        set(_macos_compile_options)

        # ad hoc: Cosmopolitan's process triple describes the packaged payload,
        # not the macOS target currently compiling this CMake target. Make the
        # target's existing CMake architecture explicit for both the real
        # compiler and compile_commands.json.
        # TODO: Move target normalization into the planned discovery tool.
        if(NOT CMAKE_CXX_COMPILER_TARGET)
            get_target_property(_macos_architectures
                ${target} OSX_ARCHITECTURES)
            if(NOT _macos_architectures
                    OR _macos_architectures STREQUAL
                        "_macos_architectures-NOTFOUND")
                set(_macos_architectures "${CMAKE_OSX_ARCHITECTURES}")
            endif()
            if(NOT _macos_architectures)
                set(_macos_architectures "${CMAKE_SYSTEM_PROCESSOR}")
            endif()
            if(NOT _macos_architectures)
                message(FATAL_ERROR
                    "omnirefl: unable to resolve the macOS architecture for "
                    "target ${target}; set OSX_ARCHITECTURES explicitly")
            endif()

            list(GET _macos_architectures 0 _macos_architecture)
            list(APPEND _macos_compile_options
                "$<$<COMPILE_LANGUAGE:CXX>:--target=${_macos_architecture}-apple-darwin>")
        endif()

        # Freestanding compilation does not imply SDK independence: it may
        # still include platform headers, and CMake has no target property that
        # describes the effective per-source include requirements. A future
        # no-SDK mode must therefore be an explicit integration contract.
        if(NOT CMAKE_OSX_SYSROOT)
            _omni_resolve_macos_sdk_fallback(_macos_sysroot)
            list(APPEND _macos_compile_options
                "$<$<COMPILE_LANGUAGE:CXX>:-isysroot${_macos_sysroot}>")
        endif()

        # BEFORE places these synthesized defaults before target and transitive
        # compile options, leaving later user-supplied options authoritative.
        # The defaults intentionally affect the real compiler so its formerly
        # implicit environment is reproduced by Omnirefl through
        # compile_commands.json.
        target_compile_options(${target} BEFORE PRIVATE
            ${_macos_compile_options})
    endif()

    set_target_properties(${target} PROPERTIES _OMNIREFL_PROCESSED TRUE)

    if(OMNIREFL_ENABLE_INDEX_MODE)
        target_compile_definitions(${target} PRIVATE OMNI_ENABLE_INDEX_MODE=1)
    endif()

    _omni_classify_target_sources(
        ${target}
        _tu_sources
        _c_sources
        _generator_expression_sources)

    if(_generator_expression_sources)
        message(FATAL_ERROR
            "omnirefl: target ${target} contains source generator expressions, "
            "which cannot be classified at configure time. Collect the sources "
            "requiring reflection in a separate C++ target and apply "
            "omni_reflected_target(...) to that target.")
    endif()

    if(_c_sources)
        list(JOIN _c_sources ", " _c_sources_text)
        message(STATUS
            "omnirefl: target ${target}: ignoring C translation units: "
            "${_c_sources_text}")
    endif()

    if(NOT _tu_sources)
        message(WARNING
            "omnirefl: target ${target} has no C++ translation units. "
            "Reflection skipped.")
        return()
    endif()

    set(_out_dir "${CMAKE_CURRENT_BINARY_DIR}/omni_${target}")
    set(_comp_db "${CMAKE_BINARY_DIR}/compile_commands.json")

    _omni_check_minimum_cxx_standard("${target}")

    message(STATUS "omnirefl: reflected target `${target}`")

    get_target_property(_target_cxx_standard ${target} CXX_STANDARD)
    if(_target_cxx_standard STREQUAL "_target_cxx_standard-NOTFOUND")
        set(_target_cxx_standard "${CMAKE_CXX_STANDARD}")
    endif()

    set(_msvc_cxx23_preview_ad_hoc 0)
    if(MSVC
            AND "${_target_cxx_standard}" STREQUAL "23"
            AND NOT OMNIREFL__DISABLE_PREVIEW_AD_HOC)
        set(_msvc_cxx23_preview_ad_hoc 1)
    endif()

    # MSVC also covers clang-cl: both use /FI and the /Yu PCH boundary.
    if(MSVC)
        # ad hoc: CMake puts source /FI options before its PCH /FI, where MSVC
        # skips reflection while seeking the /Yu boundary. Append a dispatcher
        # to CMake's PCH-use options so reflection is included after that boundary.
        # CXX_COMPILE_OPTIONS_USE_PCH is internal; it preserves CMake's resolved
        # wrapper path, including PCH reuse and configuration-specific directories.
        # The reflection-header suffix excludes this dispatcher from tool parsing.
        # Evaluate PCH availability during generation: callers may enable it later.
        string(CONCAT _msvc_pch
            "$<AND:"
                "$<NOT:$<BOOL:$<TARGET_PROPERTY:DISABLE_PRECOMPILE_HEADERS>>>,"
                "$<OR:"
                    "$<BOOL:$<TARGET_PROPERTY:PRECOMPILE_HEADERS>>,"
                    "$<BOOL:$<TARGET_PROPERTY:PRECOMPILE_HEADERS_REUSE_FROM>>"
                ">"
            ">")
        set(_pch_dispatch "${_out_dir}/internal/pch_dispatch.omnirefl.hpp")
        # file(GENERATE) has no consuming-target context; qualify its condition.
        string(REPLACE "$<TARGET_PROPERTY:" "$<TARGET_PROPERTY:${target},"
            _pch_file_condition "${_msvc_pch}")
        file(GENERATE OUTPUT "${_pch_dispatch}" CONTENT
            "#ifdef OMNIREFL_GENERATED_HEADER\n#include OMNIREFL_GENERATED_HEADER\n#endif\n"
            CONDITION "${_pch_file_condition}")
        get_target_property(_pch_options ${target} CXX_COMPILE_OPTIONS_USE_PCH)
        if(NOT _pch_options)
            set(_pch_options "${CMAKE_CXX_COMPILE_OPTIONS_USE_PCH}")
        endif()
        set_property(TARGET ${target} PROPERTY CXX_COMPILE_OPTIONS_USE_PCH
            "${_pch_options};/FI${_pch_dispatch}")
    endif()

    # Shared lists for common plumbing
    set(_gen_outputs)
    set(_gen_headers)
    set(_gen_depfiles)

    message(STATUS "omnirefl: selected generated-header reflection for target ${target}")
    message(STATUS "omnirefl: output dir for target ${target}: ${_out_dir}")

    foreach(_src IN LISTS _tu_sources)
        get_filename_component(_abs "${_src}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        string(SHA1 _src_hash "${_abs}")
        get_filename_component(_stem "${_src}" NAME_WE)

        set(_generated_header "${_out_dir}/${_stem}_${_src_hash}.omnirefl.hpp")
        set(_depfile "${_generated_header}.d")
        set(_stamp "${_generated_header}.stamp")

        file(MAKE_DIRECTORY "${_out_dir}")
        if(NOT EXISTS "${_generated_header}")
            file(WRITE "${_generated_header}"
                "// Placeholder generated by omnirefl CMake integration.\n"
                "// The real reflection header is generated by the .omni target.\n")
        endif()

        set(_no_annotations 0)
        if(OMNIREFL_NO_ANNOTATIONS)
            set(_no_annotations 1)
        endif()
        set(_timings 0)
        if(OMNIREFL_TIMINGS)
            set(_timings 1)
        endif()

        # TODO: [medium] Report instrumentation overhead per reflected target.
        # Store the latest instrumentation and compilation times per object and
        # configuration in the build tree, with separate records for parallel
        # writers. Compare instrumentation with compilation time for that
        # target's instrumented objects; unrelated targets dilute the ratio.
        # Separate this build's work from historical measurements: incremental
        # builds often skip instrumentation. Warn only when instrumentation ran
        # and a configurable threshold is exceeded (consider 50% by default).
        # Suggest isolating reflected code to limit regeneration and overhead.
        # Compilation timing may use an optional launcher, preserving existing
        # launchers. Summed operation times are not parallel build wall time;
        # report after the target builds, without claiming whole-build timing.

        # The compiler command is only available at build time from
        # compile_commands.json, so this cannot be expressed as a static
        # add_custom_command argument list. The -P bridge runs ccdb_query,
        # tokenizes its shell-quoted output, then invokes omnirefl with those
        # compiler args after `--` while preserving diagnostics.
        # Slash delimiters make the target's `.dir` a complete output-path
        # component instead of a suffix that can match another target name.
        # The depfile names the header as its target; Ninja requires that output
        # first. The stamp still forces generation after placeholder creation.
        add_custom_command(
            OUTPUT "${_generated_header}" "${_stamp}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_out_dir}"
            COMMAND ${CMAKE_COMMAND}
                -D "CCDB_QUERY=$<TARGET_FILE:omni::ccdb_query>"
                -D "OMNIREFL=$<TARGET_FILE:omni::tool>"
                -D "COMP_DB=${_comp_db}"
                -D "GENERATOR=${CMAKE_GENERATOR}"
                -D "SOURCE=${_src}"
                -D "OUTPUT_CONTAINS=/${target}.dir/"
                -D "OUT=${_generated_header}"
                -D "RESOURCE_DIR=${omnirefl_RESOURCE_DIR}"
                -D "NO_ANNOTATIONS=${_no_annotations}"
                -D "LOG_LEVEL=${OMNIREFL_LOG_LEVEL}"
                -D "TIMINGS=${_timings}"
                -D "MSVC_CXX23_PREVIEW_AD_HOC=${_msvc_cxx23_preview_ad_hoc}"
                -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/run_omnirefl_from_ccdb.cmake"
            COMMAND ${CMAKE_COMMAND} -E touch "${_stamp}"
            COMMENT "omnirefl: generating reflection for ${target}: ${_src}"
            DEPENDS "${_src}" "${_comp_db}" omni::tool omni::ccdb_query
            BYPRODUCTS "${_depfile}"
            DEPFILE "${_depfile}"
            VERBATIM
        )

        list(APPEND _gen_outputs "${_stamp}")
        list(APPEND _gen_headers "${_generated_header}")
        list(APPEND _gen_depfiles "${_depfile}")

        set(_fi_prop "OMNIREFL_FORCE_INCLUDE_${_src_hash}")

        # clangd reads compile_commands.json outside the generated-header
        # dependency graph, so the force-included header must exist immediately
        # after configuration. The placeholder is replaced by the real header
        # during the .omni generation step.
        # Resolve the path through the consuming target: one source can belong
        # to several targets with different compile definitions and metadata.
        set_target_properties(${target} PROPERTIES ${_fi_prop} "${_generated_header}")
        if(MSVC)
            set_source_files_properties("${_src}" PROPERTIES
                COMPILE_OPTIONS "/FI$<TARGET_PROPERTY:${_fi_prop}>")
            # The shared PCH dispatcher selects this translation unit's header.
            set_property(SOURCE "${_src}" APPEND PROPERTY COMPILE_DEFINITIONS
                "$<${_msvc_pch}:OMNIREFL_GENERATED_HEADER=\"$<TARGET_PROPERTY:${_fi_prop}>\">")
        else()
            set_source_files_properties("${_src}" PROPERTIES
                COMPILE_OPTIONS "-include;$<TARGET_PROPERTY:${_fi_prop}>")
        endif()
    endforeach()

    target_link_libraries(${target} PRIVATE omni::refl)

    # Common plumbing: hook outputs into build + manual force-regenerate target
    set(_gen_target "_gen.${target}.omni")
    add_custom_target(${_gen_target} DEPENDS ${_gen_outputs})
    set_target_properties(${_gen_target} PROPERTIES FOLDER "omnirefl/internal")
    add_dependencies(${target} ${_gen_target})

    set(_cfg_arg "")
    if(CMAKE_CONFIGURATION_TYPES)
        set(_cfg_arg --config $<CONFIG>)
    endif()

    add_custom_target(${target}.omni
        COMMAND ${CMAKE_COMMAND} -E rm -f ${_gen_outputs} ${_gen_headers} ${_gen_depfiles}
        COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" --target ${_gen_target} ${_cfg_arg}
        COMMENT "omnirefl: force-regenerate for ${target} [.omni]"
        VERBATIM
    )
endfunction()
