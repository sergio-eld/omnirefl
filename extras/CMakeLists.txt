cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)
project(omnirefl_projects LANGUAGES C CXX)

option(BUILD_EXAMPLES "Build the packaged examples" ON)
option(BUILD_TESTING "Build the packaged tests" ON)
option(BUILD_BENCHMARKS "Build the packaged benchmarks" ON)

if(BUILD_TESTING OR BUILD_EXAMPLES OR BUILD_BENCHMARKS)
    enable_testing()
    include("${CMAKE_CURRENT_LIST_DIR}/cmake/toolchain_checks.cmake")
endif()

if(BUILD_TESTING)
    add_subdirectory(tests "${CMAKE_BINARY_DIR}/tests")
endif()

if(BUILD_EXAMPLES)
    if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/examples/scratch/CMakeLists.txt")
        add_subdirectory(examples/scratch "${CMAKE_BINARY_DIR}/examples/scratch")
    endif()

    if(NOT "cxx_std_23" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
        message(STATUS "omnirefl: skipping sneak_peek: C++23 is not available")
    else()
        omnirefl_check_cxx23_library(_examples_has_cxx23_library)
        if(_examples_has_cxx23_library)
            add_subdirectory(examples/sneak_peek
                "${CMAKE_BINARY_DIR}/examples/sneak_peek")
            if(CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME)
                add_test(NAME example.sneak_peek COMMAND sneak_peek)
            endif()
        else()
            message(STATUS "omnirefl: skipping sneak_peek: C++23 library features are not available")
        endif()
    endif()

    if(NOT DEFINED BUILD_EXTENSIONS OR BUILD_EXTENSIONS)
        add_subdirectory(extensions/serialization/guide
            "${CMAKE_BINARY_DIR}/extensions/serialization/guide")
    else()
        message(STATUS "omnirefl: skipping guide.json: extensions disabled")
    endif()

    omnirefl_check_cxx20_language(_examples_has_guide_features)
    if(_examples_has_guide_features)
        add_subdirectory(examples/comprehensive_guide
            "${CMAKE_BINARY_DIR}/examples/comprehensive_guide")
    else()
        message(STATUS "omnirefl: skipping comprehensive_guide: C++20 language features are not available")
    endif()
endif()

if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks "${CMAKE_BINARY_DIR}/benchmarks")
endif()
