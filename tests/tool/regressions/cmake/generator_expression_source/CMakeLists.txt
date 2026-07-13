cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_generator_expression_source LANGUAGES CXX)

find_package(omnirefl CONFIG REQUIRED)

add_executable(generator_expression_source
    "$<$<BOOL:1>:${CMAKE_CURRENT_SOURCE_DIR}/generator_expression_source.cpp>")
set_target_properties(generator_expression_source PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)
omni_reflected_target(generator_expression_source LOG_LEVEL warning)
