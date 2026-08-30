# 3.18.2 is the current project floor for CMake APIs used by the package.
cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_extensions LANGUAGES CXX)

# TODO: Add opt-in components providing drop-in high-level utilities powered by
# omnirefl:
# - serialization (YAML, JSON, protobuf)
# - CLI
# - OpenAPI/REST mapping
