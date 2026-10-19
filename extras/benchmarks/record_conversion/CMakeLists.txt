add_executable(benchmark.record_conversion record_conversion.cpp)

set(_record_conversion_cpp_standard 20)
if("cxx_std_23" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    set(_record_conversion_cpp_standard 23)
endif()

set_target_properties(benchmark.record_conversion PROPERTIES
    CXX_STANDARD ${_record_conversion_cpp_standard}
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    OUTPUT_NAME record_conversion_bench)

target_link_libraries(benchmark.record_conversion PRIVATE benchmark::benchmark)

if(RECORD_CONVERSION_ENABLE_PERF_COUNTERS)
    target_compile_definitions(benchmark.record_conversion PRIVATE
        RECORD_CONVERSION_HAS_PERF_COUNTERS)
endif()

option(RECORD_CONVERSION_ENABLE_INDEX_MODE
    "Enable omnirefl index mode for the record conversion benchmark" ON)

set(_reflection_options LOG_LEVEL silent)
if(RECORD_CONVERSION_ENABLE_INDEX_MODE)
    list(APPEND _reflection_options ENABLE_INDEX_MODE)
endif()

omni_reflected_target(benchmark.record_conversion ${_reflection_options})
