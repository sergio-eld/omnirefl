# Ad hoc: the packaged frontend may target glibc while the backend uses musl.
# The compile database omits that implicit target, so resolve it once here.
# REFACTORME: [high] Move target discovery into compiler compatibility profiles.
function(_omni_resolve_compiler_target out)
    set(${out} "${CMAKE_CXX_COMPILER_TARGET}" PARENT_SCOPE)
    if(CMAKE_CXX_COMPILER_TARGET OR NOT CMAKE_SYSTEM_NAME STREQUAL "Linux"
            OR NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
        return()
    endif()
    string(SHA1 _key "${CMAKE_CXX_COMPILER};${CMAKE_CXX_COMPILER_ARG1}")
    get_property(_target GLOBAL PROPERTY "_OMNIREFL_COMPILER_TARGET_${_key}")
    if(NOT _target)
        separate_arguments(_args NATIVE_COMMAND "${CMAKE_CXX_COMPILER_ARG1}")
        execute_process(COMMAND "${CMAKE_CXX_COMPILER}" ${_args} -dumpmachine
            RESULT_VARIABLE _result
            OUTPUT_VARIABLE _target
            ERROR_VARIABLE _error
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NOT 0 EQUAL _result OR NOT _target)
            message(FATAL_ERROR "omnirefl: compiler target query failed:\n${_error}")
        endif()
        set_property(GLOBAL PROPERTY "_OMNIREFL_COMPILER_TARGET_${_key}" "${_target}")
    endif()
    set(${out} "${_target}" PARENT_SCOPE)
endfunction()

function(_omni_compiler_compatibility_flags out)
    set(${out} "" PARENT_SCOPE)
    if(MSVC OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"
            OR NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
        return()
    endif()
    # Ad hoc: the embedded frontend enables char8_t and advertises consteval
    # for older C++20 drafts whose real compiler lacks them, changing library
    # declarations and conditionally compiled reflected record layouts.
    # Compiler compatibility profiles do not yet normalize frontend defaults;
    # align these two defaults while leaving explicit source flags authoritative.
    # Hiding __cpp_consteval does not emulate older immediate-function semantics.
    # REFACTORME: [high] Replace this query with a compiler compatibility profile.
    if(NOT "cxx_std_20" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
        set(${out} "-fno-char8_t;-U__cpp_consteval" PARENT_SCOPE)
        return()
    endif()

    string(SHA1 _key
        "${CMAKE_CXX_COMPILER};${CMAKE_CXX_COMPILER_ARG1};${CMAKE_CXX_COMPILER_VERSION};${CMAKE_CXX20_STANDARD_COMPILE_OPTION}")
    set(_probe "_OMNIREFL_COMPILER_COMPATIBILITY_FLAGS_${_key}")
    get_property(_checked GLOBAL PROPERTY "${_probe}" SET)
    if(NOT _checked)
        # Query only the compiler's default. Project flags can force headers
        # found through target-only include paths, which a configure probe lacks.
        # The complete command, including those flags, is forwarded by the bridge.
        separate_arguments(_args NATIVE_COMMAND "${CMAKE_CXX_COMPILER_ARG1}")
        # An empty source file works on native Windows and cross hosts alike.
        set(_source "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/_ad_hoc_compiler_features.cpp")
        file(WRITE "${_source}" "")
        execute_process(
            COMMAND "${CMAKE_CXX_COMPILER}" ${_args}
                ${CMAKE_CXX20_STANDARD_COMPILE_OPTION} -dM -E -x c++ "${_source}"
            RESULT_VARIABLE _result
            OUTPUT_VARIABLE _macros
            ERROR_VARIABLE _error)
        if(NOT 0 EQUAL _result)
            message(FATAL_ERROR "omnirefl: compiler feature query failed:\n${_error}")
        endif()
        set(_flags)
        if(NOT _macros MATCHES "(^|\n)#define __cpp_char8_t ")
            list(APPEND _flags -fno-char8_t)
        endif()
        if(NOT _macros MATCHES "(^|\n)#define __cpp_consteval ")
            list(APPEND _flags -U__cpp_consteval)
        endif()
        set_property(GLOBAL PROPERTY "${_probe}" "${_flags}")
    endif()
    get_property(_flags GLOBAL PROPERTY "${_probe}")
    set(${out} "${_flags}" PARENT_SCOPE)
endfunction()
