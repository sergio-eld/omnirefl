cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_c_language_override LANGUAGES C CXX)

find_package(omnirefl CONFIG REQUIRED)

set_source_files_properties(main.cpp PROPERTIES LANGUAGE C)
add_executable(c_language_override main.cpp)
omni_reflected_target(c_language_override LOG_LEVEL warning)
