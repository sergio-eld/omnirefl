cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(sneak_peek LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT TARGET omni::tool)
    find_package(omnirefl CONFIG REQUIRED)
endif()

add_executable(sneak_peek main.cpp)

# Reflection is not transitive: only this target's own .cpp files are
# instrumented. Call omni_reflected_target for each target that should be
# reflected.
omni_reflected_target(sneak_peek)
