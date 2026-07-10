# 3.18.2 is the current project floor for CMake APIs used by the package and
# reflected target integration.
cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(example LANGUAGES CXX)

find_package(omnirefl CONFIG REQUIRED)

add_executable(example main.cpp)
set_property(TARGET example PROPERTY CXX_STANDARD 20)

# Reflection is not transitive: only this target's own .cpp files are
# instrumented. Call omni_reflected_target for each target that should be
# reflected.
omni_reflected_target(example)
