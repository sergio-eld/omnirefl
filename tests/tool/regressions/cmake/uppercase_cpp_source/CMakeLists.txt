cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_uppercase_cpp_source LANGUAGES CXX)

find_package(omnirefl CONFIG REQUIRED)

add_executable(uppercase_cpp_source uppercase_cpp_source.C)
set_target_properties(uppercase_cpp_source PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)
omni_reflected_target(uppercase_cpp_source LOG_LEVEL warning)
