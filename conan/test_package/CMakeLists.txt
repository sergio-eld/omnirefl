cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)
project(omnirefl_conan_consumer LANGUAGES CXX)

find_package(omnirefl CONFIG REQUIRED)

add_executable(reflection_consumer main.cpp)
set_target_properties(reflection_consumer PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON)
omni_reflected_target(reflection_consumer)
