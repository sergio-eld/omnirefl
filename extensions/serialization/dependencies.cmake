include(FetchContent)

set(_serialization_dependencies)

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
    set(RYML_INSTALL OFF CACHE BOOL "" FORCE)
    # Parse errors use invocation-local jump recovery, without exceptions.
    set(RYML_DEFAULT_CALLBACK_USES_EXCEPTIONS OFF CACHE BOOL "" FORCE)
    set(RYML_CXX_STANDARD 11 CACHE STRING "" FORCE)
    FetchContent_Declare(rapidyaml
        GIT_REPOSITORY https://github.com/biojppm/rapidyaml.git
        # v0.14.0 supports both clang-cl and GCC 16.
        GIT_TAG 11fa21d3fd3ca4a65df2c0e8b59fa0cc3b5c1642
        GIT_PROGRESS TRUE)
    list(APPEND _serialization_dependencies rapidyaml)
endif()

if(_serialization_dependencies)
    # Do not inherit the reflection tool's C++23 standard in dependencies.
    set(_serialization_parent_standard "${CMAKE_CXX_STANDARD}")
    set(CMAKE_CXX_STANDARD 11)
    FetchContent_MakeAvailable(${_serialization_dependencies})
    if(_serialization_parent_standard)
        set(CMAKE_CXX_STANDARD "${_serialization_parent_standard}")
    else()
        unset(CMAKE_CXX_STANDARD)
    endif()
endif()
