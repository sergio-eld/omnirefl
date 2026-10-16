cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_incremental_build LANGUAGES CXX)

find_package(omnirefl CONFIG REQUIRED)

add_executable(incremental_build main.cpp)
set_target_properties(incremental_build PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)
omni_reflected_target(incremental_build)
