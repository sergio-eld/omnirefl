cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)
project(ungenerated_return_type LANGUAGES CXX)

find_package(omnirefl CONFIG REQUIRED)

# Compile without instrumentation: the IDE must know return types beforehand.
foreach(standard 11 20)
    if("cxx_std_${standard}" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
        add_library(ungenerated_cpp${standard} OBJECT main.cpp)
        set_target_properties(ungenerated_cpp${standard} PROPERTIES
            CXX_STANDARD ${standard}
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO)
        target_link_libraries(ungenerated_cpp${standard} PRIVATE omni::refl)
    endif()
endforeach()
