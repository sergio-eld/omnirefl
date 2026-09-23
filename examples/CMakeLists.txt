cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_examples LANGUAGES CXX)

if(NOT "cxx_std_23" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    message(STATUS "omnirefl: skipping sneak_peek: C++23 is not available")
else()
    add_subdirectory(sneak_peek)
endif()
