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
# - enable benchmarks

find_package(Clang REQUIRED)
file(GLOB CLANG_RESOURCE_DIR "${CLANG_INCLUDE_DIRS}/../lib/clang/*/include")
if (CLANG_RESOURCE_DIR)
    get_filename_component(CLANG_RESOURCE_DIR "${CLANG_RESOURCE_DIR}/../" ABSOLUTE)
    message(STATUS "CLANG_RESOURCE_DIR: ${CLANG_RESOURCE_DIR}")
else()
    message(FATAL_ERROR "Failed to detect Clang resource dir")
endif()

# todo: use find_package
set(EXPECTED_BUILD_PACKAGE OFF CACHE INTERNAL "" FORCE)
set(EXPECTED_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
# todo: add these two to installation script
add_subdirectory(lib/expected EXCLUDE_FROM_ALL)
add_subdirectory(lib/rapidyaml EXCLUDE_FROM_ALL)

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
set_target_properties(tool PROPERTIES 
    OUTPUT_NAME ${PROJECT_NAME}
    CXX_STANDARD 20
    INTERPROCEDURAL_OPTIMIZATION ON
    ENABLE_EXPORTS OFF)
target_include_directories(tool PRIVATE ${CLANG_INCLUDE_DIRS})
# todo: fmt 
# todo: figure which clang targets really needed for linking
#   as of this writing, the resulting size is about 80M
# todo: (not here) figure out how to get rid of zlib dependency for clang
target_link_libraries(tool PRIVATE
    # clangAST
    # clangASTMatchers
    clangTooling # todo: get rid of this
    tl::expected)
target_link_static_crt(tool)
target_compile_options(tool PRIVATE -fno-rtti)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tool PRIVATE -fno-rtti -fvisibility=hidden -fdata-sections -ffunction-sections)
    target_link_options(tool PRIVATE -Wl,--gc-sections)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(tool PRIVATE /GR- /GL)
    target_link_options(tool PRIVATE /LTCG)
endif()
add_executable(omni::tool ALIAS tool)

if (ENABLE_TESTING)
    # todo: benchmarks
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    # todo: (also) test this in a sub project where `omni::tool` has been installed 
    #   and found by `find_package`
    # todo: profile build times for reflected targets.
    #   Should generate a very large file with a lot of types for serialization and
    #   profile reflection parsing and build times
    # todo: set up the path to headers
    # set path for `--resource-dir`, which will be used for clang's system headers
    set(${PROJECT_NAME}_RESOURCE_DIR "${CLANG_RESOURCE_DIR}")
    include(cmake/reflected_target.cmake)

    # currenly this is needs to be manually copied to CMakeLists.txt.in.samples
    # should figure out how to remove boilreplate for samples (they are bundled with installation)
    add_executable(test.impl_ryml samples/test/test_impl_ryml.cpp)
    reflected_target(test.impl_ryml)
    target_include_directories(test.impl_ryml PRIVATE samples/test)
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

    # tool's public headers
    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}/refl.h
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}")

    install(DIRECTORY
        # todo: proper way to include the dependency (with copyright, etc)
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/rapidyaml/src/
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/expected/include/
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}")

    # tool's cmake files
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/reflected_target.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

    # bundle clang's headers
    install(DIRECTORY
        ${CLANG_RESOURCE_DIR}/include
        # todo: add version here?
        DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/clang")

    # bundle samples
    configure_file(
        ${CMAKE_SOURCE_DIR}/CMakeLists.txt.in.samples
        ${CMAKE_BINARY_DIR}/samples/CMakeLists.txt @ONLY)
    
    install(DIRECTORY
        ${CMAKE_BINARY_DIR}/samples
        ${CMAKE_SOURCE_DIR}/samples
        DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/samples")
    
    install(IMPORTED_RUNTIME_ARTIFACTS tool)

    set(CPACK_PACKAGE_CONTACT "sergey.kolesnik.ld@gmail.com")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Sergei Kolesnik")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS YES)

    # todo: description
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
#include(cmake/reflected_target.cmake)
#add_subdirectory(sample)

# todo: clang as a static library


