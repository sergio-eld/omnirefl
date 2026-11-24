cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_reconfigure LANGUAGES CXX)

find_package(omnirefl CONFIG REQUIRED)

option(SECOND_EXTRA_DEFINITION "Change only second's compile command" OFF)
option(WITH_THIRD "Add an unrelated target" OFF)
option(THIRD_EXTRA_DEFINITION "Change third's compiler response file" OFF)

set(targets first second)
if(WITH_THIRD)
    list(APPEND targets third)
endif()

foreach(target IN LISTS targets)
    # A shared source must retain each target's own compile command.
    add_library(${target} STATIC first.cpp)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)
    omni_reflected_target(${target} LOG_LEVEL warning)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${target}.path"
        CONTENT "$<TARGET_FILE:${target}>")
endforeach()

if(SECOND_EXTRA_DEFINITION)
    target_compile_definitions(second PRIVATE EXTRA_DEFINITION=1)
endif()

if(WITH_THIRD)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/third.rsp"
        CONTENT "-DTHIRD_EXTRA=$<BOOL:${THIRD_EXTRA_DEFINITION}>\n")
    target_compile_options(third PRIVATE "@${CMAKE_CURRENT_BINARY_DIR}/third.rsp")
endif()
