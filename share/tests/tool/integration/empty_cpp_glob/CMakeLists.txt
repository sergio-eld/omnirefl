cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_empty_cpp_glob LANGUAGES C CXX)

find_package(omnirefl CONFIG REQUIRED)

# The integration harness populates this glob after the first successful build.
file(GLOB reflected_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/optional/*.cpp")

# Ad hoc: Apple ar rejects empty static archives. The C placeholder keeps the
# initial C++ source set empty while providing an archive member.
add_library(empty_cpp_glob STATIC placeholder.c ${reflected_sources})
omni_reflected_target(empty_cpp_glob LOG_LEVEL warning)
