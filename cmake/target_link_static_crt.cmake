function (target_link_static_crt target)
    if (NOT TARGET ${target})
        message(FATAL_ERROR "${target} is not a valid CMake Target!")
    endif()

    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_libraries(${target} PRIVATE -static gcc stdc++ $<$<PLATFORM_ID:Windows>:winpthread> -dynamic)
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