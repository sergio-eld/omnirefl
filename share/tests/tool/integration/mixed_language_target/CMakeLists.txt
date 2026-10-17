cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_mixed_language_target LANGUAGES C CXX)

find_package(omnirefl CONFIG REQUIRED)

add_executable(mixed_language_target main.cpp helper.c)
set_target_properties(mixed_language_target PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES)
omni_reflected_target(mixed_language_target LOG_LEVEL warning)
