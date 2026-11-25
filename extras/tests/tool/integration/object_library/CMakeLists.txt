cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_object_library LANGUAGES CXX)
find_package(omnirefl CONFIG REQUIRED)

# A shared source retains each target's reflection settings, including none.
add_library(one_field OBJECT shared.cpp)
add_library(two_fields STATIC shared.cpp)
foreach(target IN ITEMS one_field two_fields)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    target_compile_definitions(${target} PRIVATE REFLECTED_SOURCE)
    omni_reflected_target(${target} LOG_LEVEL warning)
endforeach()
target_compile_definitions(two_fields PRIVATE EXTRA_FIELD)
target_precompile_headers(two_fields PRIVATE <cstddef>)

add_executable(object_consumer main.cpp)
target_link_libraries(object_consumer PRIVATE one_field)
target_compile_definitions(object_consumer PRIVATE EXPECTED_FIELDS=1)

add_executable(injected_consumer main.cpp $<TARGET_OBJECTS:one_field>)
target_compile_definitions(injected_consumer PRIVATE EXPECTED_FIELDS=1)
omni_reflected_target(injected_consumer LOG_LEVEL warning)

add_executable(static_consumer main.cpp)
target_link_libraries(static_consumer PRIVATE two_fields)
target_compile_definitions(static_consumer PRIVATE EXPECTED_FIELDS=2)

add_executable(unreflected shared.cpp)
target_precompile_headers(unreflected PRIVATE <cstddef>)
foreach(target IN ITEMS
        object_consumer injected_consumer static_consumer unreflected)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
endforeach()
if("cxx_std_14" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    set_target_properties(unreflected PROPERTIES CXX_STANDARD 14)
endif()

if("cxx_std_17" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    add_library(standard_fields OBJECT shared.cpp)
    set_target_properties(standard_fields PROPERTIES
        CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    target_compile_definitions(standard_fields PRIVATE
        REFLECTED_SOURCE EXTRA_FIELD)
    omni_reflected_target(standard_fields LOG_LEVEL warning)

    add_executable(standard_consumer main.cpp)
    set_target_properties(standard_consumer PROPERTIES
        CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    target_link_libraries(standard_consumer PRIVATE standard_fields)
    target_compile_definitions(standard_consumer PRIVATE EXPECTED_FIELDS=3)
else()
    message(STATUS "omnirefl: skipping shared-source C++17 case: unavailable")
endif()
