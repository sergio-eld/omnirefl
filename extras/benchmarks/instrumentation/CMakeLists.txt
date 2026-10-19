set(_benchmark_cpp_standard "")
foreach(_n IN LISTS _supported_cpp_standards)
    if(_n LESS_EQUAL 23)
        set(_benchmark_cpp_standard "${_n}")
    endif()
endforeach()

if(NOT _benchmark_cpp_standard)
    message(FATAL_ERROR "omnirefl: no C++ standard up to C++23 is available for benchmark.baseline")
endif()

add_library(benchmark.baseline STATIC baseline_test.cpp)
set_target_properties(benchmark.baseline PROPERTIES
    CXX_STANDARD ${_benchmark_cpp_standard}
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

target_compile_definitions(benchmark.baseline PRIVATE CXX_STANDARD=${_benchmark_cpp_standard})
target_link_libraries(benchmark.baseline PRIVATE GTest::GTest std_compat)
omni_reflected_target(benchmark.baseline ENABLE_INDEX_MODE TIMINGS LOG_LEVEL silent)


# Measure fresh instrumentation and compilation, rather than a prebuilt target.
if(UNIX)
    enable_testing()
    find_program(_benchmark_shell sh REQUIRED)
    set(BENCHMARK_RESULTS "${CMAKE_CURRENT_BINARY_DIR}/results" CACHE PATH "Timing reports")
    set(BENCHMARK_NAME instrumentation CACHE STRING "Timing report label")
    add_test(NAME benchmark.instrumentation
        COMMAND "${_benchmark_shell}" "${CMAKE_CURRENT_LIST_DIR}/measure.sh"
            --build "${CMAKE_BINARY_DIR}"
            --results "${BENCHMARK_RESULTS}" --name "${BENCHMARK_NAME}")
endif()
