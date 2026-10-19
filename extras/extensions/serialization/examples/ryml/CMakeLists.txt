cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(json_serialization LANGUAGES CXX)

# The serialization extension supports C++11; preserve newer compiler defaults.
if(NOT DEFINED CMAKE_CXX_STANDARD
    AND CMAKE_CXX_STANDARD_COMPUTED_DEFAULT STREQUAL "98")
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(NOT TARGET omni::serialization)
    find_package(omnirefl CONFIG REQUIRED COMPONENTS serialization)
endif()

add_executable(json_serialization main.cpp)
target_link_libraries(json_serialization PRIVATE omni::serialization)
omni_reflected_target(json_serialization)
