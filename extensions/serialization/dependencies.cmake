include(FetchContent)

set(_serialization_dependencies)
set(_serialization_add_ryml_sources FALSE)

if(NOT TARGET tl::expected)
    find_package(tl-expected CONFIG QUIET)
endif()
if(NOT TARGET tl::expected)
    set(EXPECTED_BUILD_PACKAGE OFF CACHE BOOL "" FORCE)
    set(EXPECTED_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(tl_expected
        GIT_REPOSITORY https://github.com/TartanLlama/expected.git
        GIT_TAG v1.3.1
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE)
    list(APPEND _serialization_dependencies tl_expected)
endif()

if(NOT TARGET tl::optional)
    find_package(tl-optional CONFIG QUIET)
endif()
if(NOT TARGET tl::optional)
    set(OPTIONAL_BUILD_PACKAGE OFF CACHE BOOL "" FORCE)
    set(OPTIONAL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(tl_optional
        GIT_REPOSITORY https://github.com/TartanLlama/optional.git
        GIT_TAG v1.1.0
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE)
    list(APPEND _serialization_dependencies tl_optional)
endif()

if(NOT TARGET ryml::ryml)
    find_package(ryml 0.14 CONFIG QUIET)
endif()
if(NOT TARGET ryml::ryml)
    FetchContent_Declare(rapidyaml
        GIT_REPOSITORY https://github.com/biojppm/rapidyaml.git
        # v0.14.0 supports both clang-cl and GCC 16.
        GIT_TAG 11fa21d3fd3ca4a65df2c0e8b59fa0cc3b5c1642
        # Populate the source without adding RapidYAML's compiled targets.
        SOURCE_SUBDIR _omnirefl_no_cmake_build
        GIT_PROGRESS TRUE)
    list(APPEND _serialization_dependencies rapidyaml)
    set(_serialization_add_ryml_sources TRUE)
endif()

if(_serialization_dependencies)
    FetchContent_MakeAvailable(${_serialization_dependencies})
endif()

if(_serialization_add_ryml_sources)
    set(_serialization_ryml_sources
        ${rapidyaml_SOURCE_DIR}/src/c4/yml/common.cpp
        ${rapidyaml_SOURCE_DIR}/src/c4/yml/node_type.cpp
        ${rapidyaml_SOURCE_DIR}/src/c4/yml/parse.cpp
        ${rapidyaml_SOURCE_DIR}/src/c4/yml/preprocess.cpp
        ${rapidyaml_SOURCE_DIR}/src/c4/yml/reference_resolver.cpp
        ${rapidyaml_SOURCE_DIR}/src/c4/yml/tag.cpp
        ${rapidyaml_SOURCE_DIR}/src/c4/yml/tree.cpp
        ${rapidyaml_SOURCE_DIR}/src/c4/yml/version.cpp
        ${rapidyaml_SOURCE_DIR}/ext/c4core/src/c4/alloc.cpp
        ${rapidyaml_SOURCE_DIR}/ext/c4core/src/c4/base64.cpp
        ${rapidyaml_SOURCE_DIR}/ext/c4core/src/c4/char_traits.cpp
        ${rapidyaml_SOURCE_DIR}/ext/c4core/src/c4/error.cpp
        ${rapidyaml_SOURCE_DIR}/ext/c4core/src/c4/format.cpp
        ${rapidyaml_SOURCE_DIR}/ext/c4core/src/c4/language.cpp
        ${rapidyaml_SOURCE_DIR}/ext/c4core/src/c4/memory_resource.cpp
        ${rapidyaml_SOURCE_DIR}/ext/c4core/src/c4/memory_util.cpp
        ${rapidyaml_SOURCE_DIR}/ext/c4core/src/c4/utf.cpp
        ${rapidyaml_SOURCE_DIR}/ext/c4core/src/c4/version.cpp)

    add_library(omnirefl_ryml_sources INTERFACE)
    add_library(ryml::ryml ALIAS omnirefl_ryml_sources)
    target_compile_features(omnirefl_ryml_sources INTERFACE cxx_std_11)
    target_include_directories(omnirefl_ryml_sources SYSTEM INTERFACE
        ${rapidyaml_SOURCE_DIR}/src
        ${rapidyaml_SOURCE_DIR}/ext/c4core/src)
    target_sources(omnirefl_ryml_sources INTERFACE
        ${_serialization_ryml_sources})
endif()
