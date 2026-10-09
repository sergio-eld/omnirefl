cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(json_serialization LANGUAGES CXX)

# The serialization extension supports C++11; use the compiler's default here.

if(NOT TARGET omni::serialization)
    find_package(omnirefl CONFIG REQUIRED COMPONENTS serialization)
endif()

add_executable(json_serialization main.cpp)
target_link_libraries(json_serialization PRIVATE omni::serialization)
omni_reflected_target(json_serialization)
