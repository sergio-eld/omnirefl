include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
include(CheckCXXSourceCompiles)

function(_serialization_check_cxx20_span result)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

    check_cxx_source_compiles(
        "#include <span>
        int main() {
            int value = 0;
            std::span<int> view{&value, 1};
            return view.empty();
        }"
        _serialization_has_cxx20_span)

    set(${result} "${_serialization_has_cxx20_span}" PARENT_SCOPE)
endfunction()

set(_serialization_dependencies)

if(NOT TARGET tl::expected)
    set(EXPECTED_BUILD_PACKAGE OFF CACHE BOOL "" FORCE)
    set(EXPECTED_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(tl_expected
        URL https://github.com/TartanLlama/expected/archive/refs/tags/v1.3.1.tar.gz
        URL_HASH SHA256=9a04f4f472fbb5c30bf60402f1ca626c4a76987f867978d0b8a35d7ab3fb8fe7)
    list(APPEND _serialization_dependencies tl_expected)
endif()

if(NOT TARGET tl::optional)
    set(OPTIONAL_BUILD_PACKAGE OFF CACHE BOOL "" FORCE)
    set(OPTIONAL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(tl_optional
        URL https://github.com/TartanLlama/optional/archive/refs/tags/v1.1.0.tar.gz
        URL_HASH SHA256=88ece79f3de5ccaec4191951a222f95cc80c4381dafd3163bdb1ff87cedf3118)
    list(APPEND _serialization_dependencies tl_optional)
endif()

if(NOT TARGET ryml::ryml)
    set(RYML_INSTALL OFF CACHE BOOL "" FORCE)
    # Parse errors use invocation-local jump recovery, without exceptions.
    set(RYML_DEFAULT_CALLBACK_USES_EXCEPTIONS OFF CACHE BOOL "" FORCE)
    set(RYML_CXX_STANDARD 11 CACHE STRING "" FORCE)

    # c4core assumes every C++20 standard library provides <span>.
    set(_serialization_ryml_patch)
    if("cxx_std_20" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
        _serialization_check_cxx20_span(_serialization_has_cxx20_span)
        if(NOT _serialization_has_cxx20_span)
            set(_serialization_ryml_patch
                PATCH_COMMAND "${CMAKE_COMMAND}"
                    "-DSOURCE_DIR=<SOURCE_DIR>/ext/c4core"
                    -P "${CMAKE_CURRENT_LIST_DIR}/patch_c4core_span.cmake")
        endif()
    endif()

    FetchContent_Declare(rapidyaml
        # The source release includes c4core and its CMake helpers.
        URL https://github.com/biojppm/rapidyaml/releases/download/v0.14.0/rapidyaml-0.14.0-src.tgz
        URL_HASH SHA256=6e6e67e6b766c5a1c5fcb8b80292c7e7c15a3033c01730b41f4a91eb8a31767c
        ${_serialization_ryml_patch})
    list(APPEND _serialization_dependencies rapidyaml)
endif()

if(_serialization_dependencies)
    # Do not inherit the reflection tool's C++23 standard in dependencies.
    set(_serialization_parent_standard "${CMAKE_CXX_STANDARD}")
    set(CMAKE_CXX_STANDARD 11)
    set(_serialization_parent_folder "${CMAKE_FOLDER}")
    set(CMAKE_FOLDER "Dependencies")
    FetchContent_MakeAvailable(${_serialization_dependencies})
    # An empty parent must unset CMAKE_FOLDER, not create an empty IDE group.
    set(CMAKE_FOLDER ${_serialization_parent_folder})
    foreach(_dependency IN LISTS _serialization_dependencies)
        set_property(DIRECTORY "${${_dependency}_SOURCE_DIR}"
            PROPERTY EXCLUDE_FROM_ALL TRUE)
    endforeach()
    if("rapidyaml" IN_LIST _serialization_dependencies)
        # ryml overrides CMAKE_FOLDER with an empty folder of its own.
        set_property(TARGET ryml PROPERTY FOLDER "Dependencies")
    endif()
    if(_serialization_parent_standard)
        set(CMAKE_CXX_STANDARD "${_serialization_parent_standard}")
    else()
        unset(CMAKE_CXX_STANDARD)
    endif()
endif()
