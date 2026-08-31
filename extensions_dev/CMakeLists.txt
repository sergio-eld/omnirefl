# 3.18.2 is the current project floor for CMake APIs used by the package.
cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_extension_examples LANGUAGES CXX)

# The main build provides its targets before adding this project. The copy
# installed under share/ instead discovers them from the installed package.
if(NOT _in_omnirefl_build)
    find_package(omnirefl CONFIG REQUIRED)
endif()

# Extension dependencies are fetched by this dev project so source-tree and
# installed-package validation use the same dependency setup.
include(FetchContent)

# TODO: Add per-extension tests, examples, sandboxes, and benchmarks.
