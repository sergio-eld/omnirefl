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
