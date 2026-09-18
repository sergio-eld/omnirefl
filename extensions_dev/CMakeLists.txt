cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_extension_examples LANGUAGES CXX)

if(NOT TARGET omni::refl)
    find_package(omnirefl CONFIG REQUIRED)
endif()

# TODO: Consider examples and sandboxes for future extension implementations.
