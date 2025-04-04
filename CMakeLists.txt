cmake_minimum_required(VERSION 3.14.2 FATAL_ERROR)

project(omnirefl LANGUAGES CXX C VERSION 0.0.1)

# todo: what if I just define 20 globally instead?
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(cmake/clean_build.cmake)
include(cmake/target_link_static_crt.cmake)

function(add_static_compile_options TARGET)
    target_link_static_crt(${TARGET})
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${TARGET} PRIVATE -fno-rtti -fvisibility=hidden -fdata-sections -ffunction-sections)
        target_link_options(${TARGET} PRIVATE -Wl,--gc-sections)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(${TARGET} PRIVATE /GR- /GL)
        target_link_options(${TARGET} PRIVATE /LTCG)
    endif()
endfunction()

option(ENABLE_PACKAGE "Building the package" OFF)
message(STATUS "ENABLE_PACKAGE: ${ENABLE_PACKAGE}")

option(ENABLE_TESTING "Enable testing" OFF)
message(STATUS "ENABLE_TESTING: ${ENABLE_TESTING}")

option(ENABLE_BENCH "Enable benchmarks" OFF)
message(STATUS "ENABLE_BENCH: ${ENABLE_BENCH}")

include(FetchContent)
FetchContent_Declare(
    cli11_proj
    QUIET
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.3.2
)
FetchContent_MakeAvailable(cli11_proj)

list(APPEND CMAKE_MODULE_PATH ${LLVM_CMAKE_DIR})
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

# todo: enable when the tool is ready and default implementation is needed
# find_package(Python COMPONENTS Interpreter REQUIRED)
# set(RYML_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/include/ryml)
# set(RYML_AMALGAMATED_HEADER "${RYML_INCLUDE_DIR}/ryml.hpp")
# add_custom_command(OUTPUT "${RYML_AMALGAMATED_HEADER}"
#     # ad hoc, since the script needs write permissions for some reason,
#     # which prevents setting `ro` mount for source in docker
#     COMMAND ${CMAKE_COMMAND} -E copy_directory 
#         "${CMAKE_CURRENT_SOURCE_DIR}/lib/rapidyaml"
#         "${CMAKE_CURRENT_BINARY_DIR}/rapidyaml"
# 
#     COMMAND Python::Interpreter
#         "${CMAKE_CURRENT_BINARY_DIR}/rapidyaml/tools/amalgamate.py"
#         "${RYML_AMALGAMATED_HEADER}"
#     COMMENT "Amalgamate ryml.hpp to: ${RYML_AMALGAMATED_HEADER}"
#     VERBATIM)
# add_custom_target(amalgamate_ryml DEPENDS ${RYML_AMALGAMATED_HEADER})

# this library will be automatically linked to a reflected target
add_library(refl INTERFACE)
target_sources(refl INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}/refl.hpp>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}/refl.hpp>)
target_include_directories(refl INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    # fixme:
    #   This is needed because of 3rd party libs.
    #   Proper installation might be needed, however this would require version tracking...
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

# for unit testing
add_library(tool.cli STATIC tool/cli.cpp)
target_include_directories(tool.cli PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(tool.cli 
    PRIVATE CLI11::CLI11 fmt::fmt
    PUBLIC tl::expected)
set_target_properties(tool.cli PROPERTIES CXX_STANDARD 20)
add_static_compile_options(tool.cli)

# todo: implement as clang plugin
add_executable(tool 
    main.cpp
    # todo: tool/ast.h -> tool/ast/???.h
    tool/ast.cpp
    tool/ast/util.cpp
    tool/header_mode/emit_code.cpp
    tool/header_mode/header_mode.cpp
    tool/source_mode/emit_code.cpp
    tool/source_mode/source_mode.cpp
)
# fixme: this is supposed to reduce compilation time for fmt...
# target_precompile_headers(tool PRIVATE fmt/format.h)

# todo: enable when the tool is ready, to provide default implementation
# add_dependencies(tool amalgamate_ryml)
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
    tool.cli
    # clangAST
    # clangASTMatchers
    clangTooling # todo: get rid of this
    tl::expected
    fmt::fmt)
add_static_compile_options(tool)
add_executable(omni::tool ALIAS tool)

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
        # "${RYML_INCLUDE_DIR}/../"
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

    install(DIRECTORY
        ${CMAKE_SOURCE_DIR}/tests
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

# todo: disable for cross compilation, since it requires running the compiled binary
if (ENABLE_TESTING)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    if (NOT TARGET GTest::GTest)
        add_library(GTest::GTest ALIAS GTest::gtest)
    endif()
    if (NOT TARGET GTest::Main)
        add_library(GTest::Main ALIAS GTest::gtest_main)
    endif()

    # refactorme: separate directory
    add_executable(test.cli tool/tests/cli.cpp)
    set_target_properties(test.cli PROPERTIES CXX_STANDARD 20)
    # todo: implement
    # target_link_libraries(test.cli PRIVATE tool.cli GTest::GTest GTest::Main)
    # add_test(NAME test.cli COMMAND test.cli)

    add_executable(sb.cli tool/tests/sb_cli.cpp)
    target_link_libraries(sb.cli PRIVATE tool.cli)

    # todo: (also) test this in a sub project where `omni::tool` has been installed 
    #   and found by `find_package`
    # todo: profile build times for reflected targets.
    #   Should generate a very large file with a lot of types for serialization and
    #   profile reflection parsing and build times
    set(${PROJECT_NAME}_RESOURCE_DIR "${CLANG_RESOURCE_DIR}")
    include(cmake/reflected_target.cmake)
    add_subdirectory(tests)

    # todo: 
    # - (?) install the package locally (install dir within the project root)
endif()

if (ENABLE_BENCH)
    # benchmark: (move todos into .cpp file)
    # - parsing an empty source file
    # - reading files
    # - ast creation
    # - reflection engine compilation (profile templates instantiations)

    # macro bench
    add_executable(generate_structs generate_structs.cpp)
    set_target_properties(generate_structs
        PROPERTIES 
        CXX_STANDARD 20)
    target_link_libraries(generate_structs fmt::fmt)

    # todo: do I need it as a custom_target? Maybe custom_command instead
    add_custom_target(bench.structs_100.generate
        COMMAND generate_structs 100 ./bench structs_100.h test_structs_100.cpp
        # WORKING_DIRECTORY ${PROJECT_BINARY_DIR} # defaults to
        BYPRODUCTS ./bench/structs_100.h ./bench/test_structs_100.cpp
        VERBATIM)
    add_executable(bench.structs_100 ${PROJECT_BINARY_DIR}/bench/test_structs_100.cpp)

    # this will create target `bench.structs_100.omni`.
    # macro bench script (perf) must use the command generated by cmake for this target
    reflected_target(bench.structs_100)
    add_dependencies(bench.structs_100.omni bench.structs_100.generate)

    # micro bench
    set(BENCHMARK_ENABLE_INSTALL OFF)
    set(BENCHMARK_ENABLE_TESTING OFF)
    FetchContent_Declare(benchmark
      GIT_REPOSITORY https://github.com/google/benchmark.git
      GIT_TAG v1.7.1)
    FetchContent_MakeAvailable(benchmark)

    # todo: remove code duplication
    add_executable(bench.parse_ast bench/parse_ast.cpp
        tool/ast.cpp)
    target_link_static_crt(bench.parse_ast)
    # todo: it should depend on byproduct and not trigger the generation every time
    add_dependencies(bench.parse_ast bench.structs_100.generate)
    set_target_properties(bench.parse_ast PROPERTIES 
        CXX_STANDARD 20
        INTERPROCEDURAL_OPTIMIZATION ON
        ENABLE_EXPORTS OFF)
    target_include_directories(bench.parse_ast PRIVATE 
        ${CLANG_INCLUDE_DIRS} 
        ${LLVM_INCLUDE_DIRS}
        ${PROJECT_SOURCE_DIR})
    target_link_libraries(bench.parse_ast PRIVATE benchmark::benchmark
        clangTooling # todo: get rid of this
        tl::expected
        fmt::fmt)

    # fixme: pass as runtime arguments, or as relative paths (mapped volumes have different paths)
    target_compile_definitions(bench.parse_ast PRIVATE
        DB_PATH="${CMAKE_CURRENT_BINARY_DIR}"
        CPP_PATH="${CMAKE_CURRENT_BINARY_DIR}/bench/test_structs_100.cpp"
        RESOURCE_DIR="${CLANG_RESOURCE_DIR}")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # -fno-rtti is needed, otherwise linker error for clang::tooling::ToolAction
        target_compile_options(bench.parse_ast PRIVATE -fno-rtti -fvisibility=hidden -fdata-sections -ffunction-sections)
        target_link_options(bench.parse_ast PRIVATE -Wl,--gc-sections)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(bench.parse_ast PRIVATE /GR- /GL)
        target_link_options(bench.parse_ast PRIVATE /LTCG)
    endif()
endif()

