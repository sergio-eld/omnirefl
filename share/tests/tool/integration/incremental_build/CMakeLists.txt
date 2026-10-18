cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_incremental_build LANGUAGES CXX)

option(USE_PCH "Precompile the reflected model header" OFF)

find_package(omnirefl CONFIG REQUIRED)

add_executable(incremental_build main.cpp)
set_target_properties(incremental_build PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)
if(USE_PCH)
    target_precompile_headers(incremental_build PRIVATE model.hpp)
endif()
omni_reflected_target(incremental_build)
