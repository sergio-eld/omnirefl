# 3.18.2 is the current project floor for CMake APIs used by the package and
# reflected target integration.
cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(comprehensive_guide LANGUAGES CXX)

if(NOT TARGET omni::tool)
    find_package(omnirefl CONFIG REQUIRED)
endif()

add_executable(comprehensive_guide comprehensive_guide.cpp)
set_property(TARGET comprehensive_guide PROPERTY CXX_STANDARD 20)

target_link_libraries(comprehensive_guide PRIVATE GTest::GTest gtest_main)

# Reflection is not transitive: only this target's own .cpp files are
# instrumented. Call omni_reflected_target for each target that should be
# reflected.
omni_reflected_target(comprehensive_guide ENABLE_INDEX_MODE)

add_test(NAME "comprehensive_guide.run_omni"
    COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" -t comprehensive_guide.omni)

add_test(NAME "comprehensive_guide.build"
    COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" -t comprehensive_guide --parallel)

set_tests_properties("comprehensive_guide.build" PROPERTIES
    DEPENDS "comprehensive_guide.run_omni")

if(CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME)
    add_test(NAME "comprehensive_guide" COMMAND comprehensive_guide)
    set_tests_properties("comprehensive_guide" PROPERTIES
        DEPENDS "comprehensive_guide.build")
endif()
