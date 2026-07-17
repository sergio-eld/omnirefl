add_executable(relative_response_file main.cpp)
set_target_properties(relative_response_file PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
target_compile_options(relative_response_file PRIVATE
    @reflection-flags.rsp)
omni_reflected_target(relative_response_file)
