cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_relative_response_file LANGUAGES CXX)

find_package(omnirefl CONFIG REQUIRED)

file(WRITE "${CMAKE_BINARY_DIR}/reflection-flags.rsp"
    "-DOMNI_RESPONSE_FILE_EXPANDED\n")

add_subdirectory(app)
