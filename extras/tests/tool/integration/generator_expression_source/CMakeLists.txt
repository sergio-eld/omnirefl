cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_generator_expression_source LANGUAGES CXX)

find_package(omnirefl CONFIG REQUIRED)

add_executable(generator_expression_source
    "$<$<BOOL:1>:${CMAKE_CURRENT_SOURCE_DIR}/main.cpp>")
omni_reflected_target(generator_expression_source LOG_LEVEL warning)
