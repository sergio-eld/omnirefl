cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_generated_source_regression LANGUAGES CXX)

find_package(omnirefl CONFIG REQUIRED)

set(generated_source "${CMAKE_CURRENT_BINARY_DIR}/generated.cpp")
add_custom_command(
    OUTPUT "${generated_source}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/generated.cpp.in"
        "${generated_source}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/generated.cpp.in"
    VERBATIM)

add_executable(generated_source "${generated_source}")
set_property(TARGET generated_source PROPERTY CXX_STANDARD 11)
omni_reflected_target(generated_source LOG_LEVEL warning)
