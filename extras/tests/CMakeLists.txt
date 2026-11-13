cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)
project(omnirefl_tests LANGUAGES C CXX)
enable_testing()
get_filename_component(omnirefl_PROJECTS_DIR
    "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT TARGET omni::tool)
    find_package(omnirefl CONFIG REQUIRED)
endif()

include("${omnirefl_PROJECTS_DIR}/cmake/test_dependencies.cmake")
include("${omnirefl_PROJECTS_DIR}/cmake/toolchain_checks.cmake")
omnirefl_supported_cpp_standards(_supported_cpp_standards)

add_subdirectory(tool)
add_subdirectory(functional)
add_subdirectory(ccdb_query)
if(NOT DEFINED BUILD_EXTENSIONS OR BUILD_EXTENSIONS)
    if(NOT TARGET omni::serialization)
        find_package(omnirefl CONFIG REQUIRED COMPONENTS serialization)
    endif()
    add_subdirectory("${omnirefl_PROJECTS_DIR}/extensions/serialization/tests"
        extensions/serialization)
endif()
