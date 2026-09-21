include(FetchContent)
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

    # TODO(high): Make patch reapplication safe for cached source trees. When
    # the patch command changes, FetchContent reruns it without restoring the
    # modified files in RapidYAML's nested c4core submodule.
    # c4core assumes every C++20 standard library provides <span>.
    set(_serialization_ryml_patch)
    if("cxx_std_20" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
        _serialization_check_cxx20_span(_serialization_has_cxx20_span)
        if(NOT _serialization_has_cxx20_span)
            set(_serialization_ryml_patch
                PATCH_COMMAND git -C <SOURCE_DIR>/ext/c4core apply
                    "${CMAKE_CURRENT_LIST_DIR}/patches/c4core-span-availability.patch")
        endif()
    endif()

    FetchContent_Declare(rapidyaml
        GIT_REPOSITORY https://github.com/biojppm/rapidyaml.git
        # v0.14.0 supports both clang-cl and GCC 16.
        GIT_TAG 11fa21d3fd3ca4a65df2c0e8b59fa0cc3b5c1642
        ${_serialization_ryml_patch}
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
