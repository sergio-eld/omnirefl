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
# todo: options:
# - enable benchmarks

find_package(Clang REQUIRED)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "CLANG_INCLUDE_DIRS: ${CLANG_INCLUDE_DIRS}")
message(STATUS "LLVM_INCLUDE_DIRS: ${LLVM_INCLUDE_DIRS}")
file(GLOB CLANG_RESOURCE_DIR "${CLANG_INCLUDE_DIRS}/../lib/clang/*/include")
if (CLANG_RESOURCE_DIR)
    get_filename_component(CLANG_RESOURCE_DIR "${CLANG_RESOURCE_DIR}/../" ABSOLUTE)
    message(STATUS "CLANG_RESOURCE_DIR: ${CLANG_RESOURCE_DIR}")
else()
    message(FATAL_ERROR "Failed to detect Clang resource dir")
endif()

# todo: use find_package or FetchContent
set(EXPECTED_BUILD_PACKAGE OFF CACHE INTERNAL "" FORCE)
set(EXPECTED_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
add_subdirectory(lib/expected EXCLUDE_FROM_ALL)
add_subdirectory(lib/variant EXCLUDE_FROM_ALL)
add_subdirectory(lib/pattern_matching EXCLUDE_FROM_ALL)

# todo: suppress dev warning from the subdirectory
set(OPTIONAL_BUILD_TESTS OFF)
add_subdirectory(lib/optional EXCLUDE_FROM_ALL)
add_subdirectory(lib/string-view-lite EXCLUDE_FROM_ALL)

set(FMT_INSTALL OFF)
add_subdirectory(lib/fmt EXCLUDE_FROM_ALL)

find_package(Python COMPONENTS Interpreter REQUIRED)
set(RYML_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/include/ryml)
set(RYML_AMALGAMATED_HEADER "${RYML_INCLUDE_DIR}/ryml.hpp")
add_custom_command(OUTPUT "${RYML_AMALGAMATED_HEADER}"
    # ad hoc, since the script needs write permissions for some reason,
    # which prevents setting `ro` mount for source in docker
    COMMAND ${CMAKE_COMMAND} -E copy_directory 
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/rapidyaml"
        "${CMAKE_CURRENT_BINARY_DIR}/rapidyaml"

    COMMAND Python::Interpreter
        "${CMAKE_CURRENT_BINARY_DIR}/rapidyaml/tools/amalgamate.py"
        "${RYML_AMALGAMATED_HEADER}"
    COMMENT "Amalgamate ryml.hpp to: ${RYML_AMALGAMATED_HEADER}"
    VERBATIM)
add_custom_target(amalgamate_ryml DEPENDS ${RYML_AMALGAMATED_HEADER})

# this library will be automatically linked to a reflected target
add_library(refl INTERFACE)
target_sources(refl INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}/refl.hpp>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}/refl.hpp>)
target_include_directories(refl INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${RYML_INCLUDE_DIR}/../> # /include/ryml -> include/
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    # fixme: this is needed because of 3rd party libs. Proper installation might be needed, 
    #   however this would require version tracking...
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>/${PROJECT_NAME}
    )
target_link_libraries(refl INTERFACE
    # default implementation for iteration 1
    # todo: separate default implementation to a target
    $<BUILD_INTERFACE:tl::expected>
    $<BUILD_INTERFACE:tl::optional>
    $<BUILD_INTERFACE:mpark_variant>
    $<BUILD_INTERFACE:nonstd::string-view-lite>
    $<BUILD_INTERFACE:eld::pattern_matching>
    )
add_library(omni::refl ALIAS refl)

# todo: implement as clang plugin
add_executable(tool 
    main.cpp
    tool/parse_cli.cpp
    tool/tool_impl.cpp
    tool/reflection.cpp
    tool/emit_code.cpp)
add_dependencies(tool amalgamate_ryml)
set_target_properties(tool PROPERTIES 
    OUTPUT_NAME ${PROJECT_NAME}
    CXX_STANDARD 20
    INTERPROCEDURAL_OPTIMIZATION ON
    ENABLE_EXPORTS OFF)
target_include_directories(tool PRIVATE 
    ${CLANG_INCLUDE_DIRS} 
    ${LLVM_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}
)
# todo: (not here) figure out how to get rid of zlib dependency for clang
target_link_libraries(tool PRIVATE
    # clangAST
    # clangASTMatchers
    clangTooling # todo: get rid of this
    tl::expected
    fmt::fmt)
target_link_static_crt(tool)
set_target_properties(tool PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tool PRIVATE -fno-rtti -fvisibility=hidden -fdata-sections -ffunction-sections)
    target_link_options(tool PRIVATE -Wl,--gc-sections)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(tool PRIVATE /GR- /GL)
    target_link_options(tool PRIVATE /LTCG)
endif()
add_executable(omni::tool ALIAS tool)

# todo: disable for cross compilation, since it requires running the compiled binary
if (ENABLE_TESTING)
    # todo: benchmarks
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    # todo: (also) test this in a sub project where `omni::tool` has been installed 
    #   and found by `find_package`
    # todo: profile build times for reflected targets.
    #   Should generate a very large file with a lot of types for serialization and
    #   profile reflection parsing and build times
    set(${PROJECT_NAME}_RESOURCE_DIR "${CLANG_RESOURCE_DIR}")
    include(cmake/reflected_target.cmake)

    # todo: add benchmark `test/simple/bench.cpp`
    add_executable(test.simple samples/test/simple/test.cpp)
    reflected_target(test.simple)
    target_include_directories(test.simple PRIVATE samples/test/simple)
    target_link_libraries(test.simple PRIVATE GTest::gtest)
    set_target_properties(test.simple PROPERTIES
        # todo: test with C++11
        CXX_STANDARD 14
    )
    add_test(NAME test.simple COMMAND test.simple)

    add_executable(test.odr 
        samples/test/odr/test.cpp
        samples/test/odr/impl_a.cpp
        samples/test/odr/impl_b.cpp)
    reflected_target(test.odr)
    target_include_directories(test.odr PRIVATE samples/test/odr)
    target_link_libraries(test.odr PRIVATE GTest::gtest)
    set_target_properties(test.odr PROPERTIES
        CXX_STANDARD 11
    )
    add_test(NAME test.odr COMMAND test.odr)

    # todo: add implementation later
    # # currenly this is needs to be manually copied to CMakeLists.txt.in.samples
    # # should figure out how to remove boilreplate for samples (they are bundled with installation)
    # add_executable(test.impl_ryml samples/test/test_impl_ryml.cpp)
    # # todo: this should be tested and benchmarked
    # reflected_target(test.impl_ryml)
    # target_include_directories(test.impl_ryml PRIVATE samples/test)
    # target_link_libraries(test.impl_ryml PRIVATE GTest::gtest)
    # add_test(NAME test.impl_ryml COMMAND test.impl_ryml)

    # todo: add test for failure
endif()

if (ENABLE_PACKAGE)
    message(STATUS "Generating ${PROJECT_NAME} package")
    include(GNUInstallDirs)

    install(TARGETS tool refl
        EXPORT "${PROJECT_NAME}Targets"
        # todo: are these correct? Should I append ${PROJECT_NAME}?
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    # tool's public headers
    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}/refl.hpp
        # todo: default implementation for iteration 2
        # ${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}/impl_ryml.h
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}")

    # todo: dependencies on 3rd party should be optional, so should be the default implementation
    install(DIRECTORY
        # todo: proper way to include the dependency (with copyright, etc)
        "${RYML_INCLUDE_DIR}/../"
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/expected/include/
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/variant/include/
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/optional/include/
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/string-view-lite/include/
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/pattern_matching/include/
        # todo: check for the outcome
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}")

    # tool's cmake files
    install(EXPORT "${PROJECT_NAME}Targets"
        FILE "${PROJECT_NAME}Targets.cmake"
        NAMESPACE omni::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        VERSION "${version}"
        COMPATIBILITY AnyNewerVersion)

    configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

    # todo: make sure `omni::tool` target is available regardless of the cmake build type!
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/reflected_target.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

    # bundle clang's headers
    install(DIRECTORY
        ${CLANG_RESOURCE_DIR}/include
        DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/clang/${LLVM_VERSION}")

    # bundle samples
    configure_file(
        ${CMAKE_SOURCE_DIR}/CMakeLists.txt.in.samples
        ${CMAKE_BINARY_DIR}/samples/CMakeLists.txt @ONLY)
    
    # todo: what's in the binary dir...?
    install(DIRECTORY
        ${CMAKE_BINARY_DIR}/samples
        ${CMAKE_SOURCE_DIR}/samples
        DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}")
    
    # this is hardly needed for an alpine build
    install(IMPORTED_RUNTIME_ARTIFACTS tool)

    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_PACKAGE_CONTACT "sergey.kolesnik.ld@gmail.com")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Sergei Kolesnik")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS YES)
    # todo: deduce from the target arch
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    set(CPACK_PACKAGE_ARCHITECTURE "amd64")

    # todo: description
    include(CPack)
endif()
# if enabled benchmarking:
# benchmark: (move todos into .cpp file)
# - parsing an empty source file
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


