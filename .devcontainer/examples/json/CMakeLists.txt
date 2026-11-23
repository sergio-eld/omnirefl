# Local JSON/YAML sandbox, seeded without overwriting subsequent edits.
cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)
project(scratch_json LANGUAGES CXX)

# Change to 11 to try the guarded compatibility examples in main.cpp.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT TARGET omni::serialization)
    find_package(omnirefl CONFIG REQUIRED COMPONENTS serialization)
endif()

add_executable(scratch_json main.cpp)
target_link_libraries(scratch_json PRIVATE omni::serialization)
omni_reflected_target(scratch_json)
