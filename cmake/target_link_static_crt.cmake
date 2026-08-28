include(CheckCSourceCompiles)

check_c_source_compiles("
#ifndef __COSMOPOLITAN__
#error not Cosmopolitan
#endif
int main(void) { return 0; }
" COSMOPOLITAN_DETECTED)

set(MUSL_TEST_CODE "
#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
    #include <features.h>
    #ifndef __USE_GNU
        #define __MUSL__
    #endif
    #undef _GNU_SOURCE /* don't contaminate other includes unnecessarily */
#else
    #include <features.h>
    #ifndef __USE_GNU
        #define __MUSL__
    #endif
#endif

int main() {
    #ifdef __MUSL__
    return 0;  // musl detected
    #else
    return 1;  // musl not detected
    #endif
}
")

if(COSMOPOLITAN_DETECTED)
    message(STATUS "Detected Cosmopolitan libc.")
    set(MUSL_DETECTED FALSE)
else()
    # Write the test code to a temporary file
    file(WRITE "${CMAKE_BINARY_DIR}/musl_test.c" "${MUSL_TEST_CODE}")

    # Try to compile and run the test code
    try_run(
        RESULT_VAR
        COMPILE_VAR
        "${CMAKE_BINARY_DIR}"
        "${CMAKE_BINARY_DIR}/musl_test.c"
    )

    # Keep the configure-time probe result in directory scope for
    # target_link_static_crt() below.
    if(RESULT_VAR EQUAL 0)
        message(STATUS "Detected musl libc.")
        add_definitions(-DUSING_MUSL)
        set(MUSL_DETECTED TRUE)
    else()
        message(STATUS "musl libc not detected (probably using glibc or another libc).")
        set(MUSL_DETECTED FALSE)
    endif()
endif ()

function (target_link_static_crt target)
    if (NOT TARGET ${target})
        message(FATAL_ERROR "${target} is not a valid CMake Target!")
    endif()

    if(COSMOPOLITAN_DETECTED)
        message(STATUS "Cosmopolitan supplies the static runtime for target '${target}'")
        set_property(TARGET ${target} PROPERTY OMNIREFL_STATIC_COSMOPOLITAN TRUE)
        return()
    endif()

    if (MUSL_DETECTED)
        message(STATUS "Fully static link with musl libc for target '${target}'")
        target_link_options(${target} PRIVATE -static)
        # Packaging queries target properties so notices follow actual link
        # behavior rather than this file's global probe state.
        set_property(TARGET ${target} PROPERTY OMNIREFL_STATIC_MUSL TRUE)
        if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            set_property(TARGET ${target} PROPERTY
                OMNIREFL_STATIC_GCC_RUNTIME TRUE)
        endif()
        return()
    endif()

    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # target_link_libraries(${target} PRIVATE -static gcc stdc++ $<$<PLATFORM_ID:Windows>:winpthread> -dynamic)
        target_link_options(${target} PRIVATE -static-libgcc -static-libstdc++)
        set_property(TARGET ${target} PROPERTY
            OMNIREFL_STATIC_GCC_RUNTIME TRUE)
                                
        if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
            target_link_libraries(${target} PRIVATE -static -lpthread)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        if (MSVC_VERSION GREATER_EQUAL 1900)
            target_compile_options(${target} PRIVATE "/MT$<$<CONFIG:Debug>:d>")
        else()
            message(FATAL_ERROR "target_link_static_crt() is only available with Visual Studio 14 2015 and later.")
        endif()
    endif()
endfunction()

# function(link_static_crt)
# TODO: implement
# endfunction()
