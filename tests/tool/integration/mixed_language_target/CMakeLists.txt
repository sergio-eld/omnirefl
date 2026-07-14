cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_mixed_language_target LANGUAGES C CXX)

find_package(omnirefl CONFIG REQUIRED)

add_executable(mixed_language_target main.cpp helper.c)
omni_reflected_target(mixed_language_target LOG_LEVEL warning)
