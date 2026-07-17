cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_source_shapes LANGUAGES CXX)

find_package(omnirefl CONFIG REQUIRED)

function(reflect_executable target)
    add_executable(${target} ${ARGN})
    set_property(TARGET ${target} PROPERTY CXX_STANDARD 11)
    omni_reflected_target(${target} LOG_LEVEL warning)
endfunction()

set(generated_absolute "${CMAKE_CURRENT_BINARY_DIR}/generated_absolute.cpp")
add_custom_command(
    OUTPUT "${generated_absolute}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/generated_absolute.cpp.in"
        "${generated_absolute}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/generated_absolute.cpp.in"
    VERBATIM)

add_custom_command(
    OUTPUT generated_relative.cpp
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/generated_relative.cpp.in"
        generated_relative.cpp
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/generated_relative.cpp.in"
    VERBATIM)

reflect_executable(generated_helpers
    generated_helpers.cpp
    "${generated_absolute}"
    generated_relative.cpp)

reflect_executable(uppercase_cpp uppercase_cpp.C)

set_source_files_properties(language_cxx.c PROPERTIES LANGUAGE CXX)
reflect_executable(language_cxx language_cxx.c)
