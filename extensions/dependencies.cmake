include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(_extensions_dependencies)

if(NOT TARGET tl::expected)
    set(EXPECTED_BUILD_PACKAGE OFF CACHE BOOL "" FORCE)
    set(EXPECTED_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(tl_expected
        URL https://github.com/TartanLlama/expected/archive/refs/tags/v1.3.1.tar.gz
        URL_HASH SHA256=9a04f4f472fbb5c30bf60402f1ca626c4a76987f867978d0b8a35d7ab3fb8fe7)
    list(APPEND _extensions_dependencies tl_expected)
endif()

if(NOT TARGET tl::optional)
    set(OPTIONAL_BUILD_PACKAGE OFF CACHE BOOL "" FORCE)
    set(OPTIONAL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(tl_optional
        URL https://github.com/TartanLlama/optional/archive/refs/tags/v1.1.0.tar.gz
        URL_HASH SHA256=88ece79f3de5ccaec4191951a222f95cc80c4381dafd3163bdb1ff87cedf3118
        # value()'s explicit throw prevents emplace() under -fno-exceptions.
        PATCH_COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=<SOURCE_DIR>"
            -P "${CMAKE_CURRENT_LIST_DIR}/serialization/patch_tl_optional.cmake")
    list(APPEND _extensions_dependencies tl_optional)
endif()

if(NOT TARGET nonstd::string-view-lite)
    set(STRING_VIEW_LITE_OPT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(STRING_VIEW_LITE_OPT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(string_view_lite
        URL https://github.com/nonstd-lite/string-view-lite/archive/refs/tags/v1.8.1.tar.gz
        URL_HASH SHA256=97b812275f07d02592efb092bd70366d310b2294f677644ea07828e6138b091d
        # Keep fallback char ordering consistent with unsigned-byte memcmp.
        PATCH_COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=<SOURCE_DIR>"
            -P "${CMAKE_CURRENT_LIST_DIR}/patch_string_view_lite.cmake")
    list(APPEND _extensions_dependencies string_view_lite)
endif()

if(_extensions_dependencies)
    # Keep dependency projects at the extensions' C++11 baseline.
    set(_extensions_parent_standard "${CMAKE_CXX_STANDARD}")
    set(CMAKE_CXX_STANDARD 11)
    set(_extensions_parent_folder "${CMAKE_FOLDER}")
    set(CMAKE_FOLDER "Dependencies")
    FetchContent_MakeAvailable(${_extensions_dependencies})
    set(CMAKE_FOLDER ${_extensions_parent_folder})
    foreach(_dependency IN LISTS _extensions_dependencies)
        set_property(DIRECTORY "${${_dependency}_SOURCE_DIR}"
            PROPERTY EXCLUDE_FROM_ALL TRUE)
    endforeach()
    if(_extensions_parent_standard)
        set(CMAKE_CXX_STANDARD "${_extensions_parent_standard}")
    else()
        unset(CMAKE_CXX_STANDARD)
    endif()
endif()
