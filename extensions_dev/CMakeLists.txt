# 3.18.2 is the current project floor for CMake APIs used by the package.
cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_extension_examples LANGUAGES CXX)

# The main build provides its targets before adding this project. The copy
# installed under share/ instead discovers them from the installed package.
if(NOT _in_omnirefl_build)
    find_package(omnirefl CONFIG REQUIRED)
endif()

# TODO: Add per-extension tests, examples, sandboxes, and benchmarks.
# TODO: Keep generic extensions available before C++23 by fetching
# `tl::expected` only when `std::expected` is unavailable.

if("cxx_std_23" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    # Extension dependencies are fetched by this dev project so source-tree
    # and installed-package validation use the same dependency setup.
    include(FetchContent)

    set(CMAKE_CXX_STANDARD 23)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(INSTALL_GMOCK OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        # The newest GoogleTest release supporting C++11, kept in sync with
        # the main test project.
        GIT_TAG release-1.12.1)

    set(RYML_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(rapidyaml
        GIT_REPOSITORY https://github.com/biojppm/rapidyaml.git
        # Preserve the dependency revision used by the recovered implementation.
        GIT_TAG 837255cfc4c62e1abf8b19c6f6068cd9246f125f
        GIT_PROGRESS TRUE)
    FetchContent_MakeAvailable(googletest rapidyaml)

    if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
        enable_testing()
    endif()

    add_executable(sb.serialization.yaml serialization_yaml.cpp)
    set_target_properties(sb.serialization.yaml PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)
    target_link_libraries(sb.serialization.yaml PRIVATE
        gtest_main
        omni::refl
        ryml::ryml)
    omni_reflected_target(sb.serialization.yaml)
    add_test(NAME sb.serialization.yaml COMMAND sb.serialization.yaml)
endif()
