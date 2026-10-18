cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_pch_shared_source LANGUAGES CXX)
find_package(omnirefl CONFIG REQUIRED)

foreach(target IN ITEMS one_field two_fields)
    add_executable(${target} main.cpp)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)
    target_precompile_headers(${target} PRIVATE model.hpp)
    omni_reflected_target(${target})
endforeach()

target_compile_definitions(one_field PRIVATE EXTRA_FIELD=0)
target_compile_definitions(two_fields PRIVATE EXTRA_FIELD=1)
