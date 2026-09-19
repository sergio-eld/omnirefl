foreach(_standard IN LISTS _supported_cpp_standards)
    set(_test_target "cpp_${_standard}.extensions.serialization")
    add_executable(${_test_target} test.cpp)
    set_target_properties(${_test_target} PROPERTIES
        CXX_STANDARD ${_standard}
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)
    # Exercise serialization and parser recovery without exceptions.
    if("GNU" STREQUAL CMAKE_CXX_COMPILER_FRONTEND_VARIANT)
        target_compile_options(${_test_target} PRIVATE -fno-exceptions)
    endif()
    target_link_libraries(${_test_target} PRIVATE
        gtest_main omni::serialization)
    omni_reflected_target(${_test_target})
    if(CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME)
        add_test(NAME ${_test_target} COMMAND ${_test_target})
    endif()

    if(ENABLE_BENCH)
        set(_benchmark_target "cpp_${_standard}.benchmark.serialization")
        add_executable(${_benchmark_target} benchmark.cpp)
        set_target_properties(${_benchmark_target} PROPERTIES
            CXX_STANDARD ${_standard}
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO)
        if("GNU" STREQUAL CMAKE_CXX_COMPILER_FRONTEND_VARIANT)
            target_compile_options(${_benchmark_target} PRIVATE -fno-exceptions)
        endif()
        target_link_libraries(${_benchmark_target} PRIVATE
            benchmark::benchmark_main omni::serialization)
        omni_reflected_target(${_benchmark_target} LOG_LEVEL silent)
    endif()
endforeach()
