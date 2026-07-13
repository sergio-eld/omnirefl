cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_dollar_path_regression LANGUAGES CXX)

find_package(omnirefl CONFIG REQUIRED)

add_executable(dollar_path main.cpp)
set_property(TARGET dollar_path PROPERTY CXX_STANDARD 11)
omni_reflected_target(dollar_path LOG_LEVEL warning)
