cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_empty_cpp_glob LANGUAGES CXX)

find_package(omnirefl CONFIG REQUIRED)

# The integration harness populates this glob after the first successful build.
file(GLOB reflected_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/optional/*.cpp")

add_library(empty_cpp_glob STATIC placeholder.hpp ${reflected_sources})
set_property(TARGET empty_cpp_glob PROPERTY LINKER_LANGUAGE CXX)
omni_reflected_target(empty_cpp_glob LOG_LEVEL warning)
