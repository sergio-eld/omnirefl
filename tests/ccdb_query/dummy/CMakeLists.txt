cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(ccdb_query_dummy LANGUAGES CXX)

add_library(unique_target STATIC unique.cpp)
target_compile_definitions(unique_target PRIVATE
    UNIQUE_TARGET=1
    "CCDB_BACKSLASH=R\"(left\\right)\"")

add_library(alpha STATIC shared.cpp)
target_compile_definitions(alpha PRIVATE ALPHA_TARGET=1)

add_library(beta STATIC shared.cpp)
target_compile_definitions(beta PRIVATE BETA_TARGET=1)
