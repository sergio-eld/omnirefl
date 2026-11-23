cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_force_regenerate LANGUAGES CXX)

find_package(omnirefl CONFIG REQUIRED)

# Three generated paths per source exceed Windows' command-line limit.
set(sources)
foreach(index RANGE 1 60)
    set(source "${CMAKE_CURRENT_BINARY_DIR}/src/unit_${index}.cpp")
    file(WRITE "${source}"
        "struct unit_${index} { int value; };\n"
        "int use_${index}() { return unit_${index}{${index}}.value; }\n")
    list(APPEND sources "${source}")
endforeach()

add_library(many STATIC ${sources})
set_target_properties(many PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)
omni_reflected_target(many LOG_LEVEL warning)
