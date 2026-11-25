# Standalone A/B workload for compare.py. The normal benchmark project retains
# its original 100-field round-trip probe below.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.18.2)
    project(omnirefl_record_shape_measurement LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD_REQUIRED YES)
    set(CMAKE_CXX_EXTENSIONS NO)
    find_package(omnirefl CONFIG REQUIRED COMPONENTS serialization)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    execute_process(COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/generate.py" "${CMAKE_CURRENT_BINARY_DIR}"
        RESULT_VARIABLE _generated)
    if(NOT _generated EQUAL 0)
        message(FATAL_ERROR "Could not generate the compile-cost workload")
    endif()
    foreach(_width 20 80 160)
        foreach(_direction read write)
            set(_target "flat_${_width}_${_direction}")
            add_library(${_target} STATIC
                "${CMAKE_CURRENT_BINARY_DIR}/${_target}.cpp")
            target_link_libraries(${_target} PRIVATE omni::serialization)
            target_include_directories(${_target} BEFORE PRIVATE
                "${OMNI_PERF_SOURCE_DIR}/include"
                "${OMNI_PERF_SOURCE_DIR}/extensions/include"
                "${OMNI_PERF_SOURCE_DIR}/extensions/serialization/include")
            omni_reflected_target(${_target} LOG_LEVEL warning)
        endforeach()
    endforeach()
    return()
endif()

# Compile-cost probes are opt-in targets and never part of an all build.
foreach(_standard IN LISTS _supported_cpp_standards)
    set(_target "cpp_${_standard}.benchmark.record_shape_compile_cost")
    add_executable(${_target} EXCLUDE_FROM_ALL main.cpp)
    set_target_properties(${_target} PROPERTIES
        CXX_STANDARD ${_standard}
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)
    target_link_libraries(${_target} PRIVATE omni::serialization)
    omni_reflected_target(${_target} LOG_LEVEL warning)
endforeach()
