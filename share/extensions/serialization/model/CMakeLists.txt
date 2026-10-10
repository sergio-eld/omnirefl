# Shared record types and JSON/YAML inputs for serialization tests and benchmarks.
add_library(json_model INTERFACE)
target_include_directories(json_model INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
