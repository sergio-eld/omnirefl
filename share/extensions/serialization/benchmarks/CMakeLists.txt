if(NOT TARGET json_model)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../model" json_model)
endif()

foreach(_standard IN LISTS _supported_cpp_standards)
    set(_target "cpp_${_standard}.benchmark.serialization")
    add_executable(${_target} benchmark.cpp)
    set_target_properties(${_target} PROPERTIES
        CXX_STANDARD ${_standard} CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    if("GNU" STREQUAL CMAKE_CXX_COMPILER_FRONTEND_VARIANT)
        target_compile_options(${_target} PRIVATE -fno-exceptions)
    endif()
    target_link_libraries(${_target} PRIVATE
        benchmark::benchmark_main omni::serialization json_model)
    omni_reflected_target(${_target} LOG_LEVEL silent)
endforeach()
