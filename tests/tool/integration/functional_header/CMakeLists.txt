cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_functional_header LANGUAGES CXX)

find_package(omnirefl CONFIG REQUIRED)

add_executable(functional_header
    main.cpp
    reflection_first.cpp
    traits_header.cpp)
set_target_properties(functional_header PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)
target_link_libraries(functional_header PRIVATE omni::refl)
