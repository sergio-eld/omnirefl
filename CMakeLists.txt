cmake_minimum_required(VERSION 3.14.2 FATAL_ERROR)

project(omnirefl LANGUAGES CXX C VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(cmake/clean_build.cmake)
include(cmake/target_link_static_crt.cmake)

option(ENABLE_PACKAGE "Building the package" OFF)
message(STATUS "ENABLE_PACKAGE: ${ENABLE_PACKAGE}")

option(ENABLE_TESTING "Enable testing" OFF)
message(STATUS "ENABLE_TESTING: ${ENABLE_TESTING}")
# todo: options
# - build clang as a static library
# - enable benchmarks

find_package(Clang REQUIRED)
find_package(LLVM REQUIRED)

# todo: use find_package
set(EXPECTED_BUILD_PACKAGE OFF CACHE INTERNAL "" FORCE)
set(EXPECTED_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
# todo: add these two to installation script
add_subdirectory(lib/expected EXCLUDE_FROM_ALL)
add_subdirectory(lib/rapidyaml EXCLUDE_FROM_ALL)

# todo: benchmarks
# todo: gtest

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# this library will be automatically linked to a reflected target
add_library(refl INTERFACE)
add_library(omni::refl ALIAS refl)
target_sources(refl INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}/refl.h>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}/refl.h>)
# set(PUBLIC_HEADERS include/omnirefl/refl.h)
# set_target_properties(refl PROPERTIES PUBLIC_HEADER "${PUBLIC_HEADERS}")
target_include_directories(refl INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(refl INTERFACE
    # default implementation for iteration 1
    ryml
    tl::expected)

# todo: implement as clang plugin
add_executable(tool main.cpp)
set_target_properties(tool PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
add_executable(omni::tool ALIAS tool)
set_property(TARGET tool PROPERTY CXX_STANDARD 20)
target_include_directories(tool PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
target_compile_definitions(tool PRIVATE ${LLVM_DEFINITIONS})
# todo: fmt 
target_link_libraries(tool PRIVATE clangTooling tl::expected)
# target_link_static_crt(tool) # todo: static libLLVM

if (ENABLE_TESTING)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    # todo: (also) test this in a sub project where `omni::tool` has been installed 
    #   and found by `find_package`
    # todo: profile build times for reflected targets.
    #   Should generate a very large file with a lot of types for serialization and
    #   profile reflection parsing and build times
    include(cmake/reflected_target.cmake)
    add_executable(test.impl_ryml 
        test/test_impl_ryml.cpp)
    reflected_target(test.impl_ryml)

    target_include_directories(test.impl_ryml PRIVATE test)
    target_link_libraries(test.impl_ryml PRIVATE GTest::gtest)
endif()

if (ENABLE_PACKAGE)
    message(STATUS "Generating ${PROJECT_NAME} package")

    include(GNUInstallDirs)

    install(TARGETS tool refl
        EXPORT "${PROJECT_NAME}Targets"
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    install(EXPORT "${PROJECT_NAME}Targets"
        FILE "${PROJECT_NAME}Targets.cmake"
        NAMESPACE omni::
        DESTINATION cmake)

    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        VERSION "${version}"
        COMPATIBILITY AnyNewerVersion)

    configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        INSTALL_DESTINATION cmake)

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/reflected_target.cmake"
        DESTINATION cmake)

    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}/refl.h
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}")

    set(CPACK_PACKAGE_CONTACT "sergey.kolesnik.ld@gmail.com")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Sergei Kolesnik")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

    # todo: description
    # todo: dependency on clang libLLVM
    include(CPack)
endif()
# if enabled benchmarking:
# benchmark: (move todos into .cpp file)
# - reading files
# - ast creation

# if enabled testing
# todo: 
# - install the package locally (install dir within the project root)
# - (?) invoke cmake as subcommand
# - use a cmake toolchain file for compilation benchmarking
# - use perf to benchmark the generaion process
# - call find_package
include(cmake/reflected_target.cmake)
add_subdirectory(sample)

# todo: clang as a static library


