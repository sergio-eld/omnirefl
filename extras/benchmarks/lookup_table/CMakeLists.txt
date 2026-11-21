if(NOT TARGET nonstd::string-view-lite)
    if(DEFINED omnirefl_DIR)
        include("${omnirefl_DIR}/components/dependencies.cmake")
    else()
        include("${omnirefl_PROJECTS_DIR}/../extensions/dependencies.cmake")
    endif()
endif()

add_custom_target(lookup_table.benchmark)
foreach(_standard IN LISTS _supported_cpp_standards)
    set(_target "cpp_${_standard}.lookup_table.benchmark")
    add_executable(${_target} main.cpp)
    set_target_properties(${_target} PROPERTIES
        CXX_STANDARD ${_standard}
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        OUTPUT_NAME "lookup_table_bench_cpp_${_standard}")
    target_link_libraries(${_target} PRIVATE
        omni::refl nonstd::string-view-lite)
    add_dependencies(lookup_table.benchmark ${_target})
endforeach()
