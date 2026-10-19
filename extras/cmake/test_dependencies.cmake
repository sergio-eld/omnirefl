include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# Enable FetchContent logging
set(FETCHCONTENT_QUIET OFF CACHE BOOL "" FORCE)

# The tests use Google Test without Google Mock.
set(BUILD_GMOCK OFF CACHE BOOL "Disable unused Google Mock targets" FORCE)
# do not let googletest install itself when run "cmake --install" / CPack
set(INSTALL_GTEST OFF CACHE BOOL "Disable installation of googletest" FORCE)
set(INSTALL_GMOCK OFF CACHE BOOL "Disable installation of googlemock" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(googletest
    # Last Google Test release supporting C++11.
    URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.tar.gz
    URL_HASH SHA256=81964fe578e9bd7c94dfdb09c8e4d6e6759e19967e397dbea48d1c10e45d0df2)

# std-compat utility
set(EXPECTED_BUILD_PACKAGE OFF CACHE BOOL "" FORCE)
set(EXPECTED_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(tl_expected
    URL https://github.com/TartanLlama/expected/archive/refs/tags/v1.3.1.tar.gz
    URL_HASH SHA256=9a04f4f472fbb5c30bf60402f1ca626c4a76987f867978d0b8a35d7ab3fb8fe7)

set(OPTIONAL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(tl_optional
    URL https://github.com/TartanLlama/optional/archive/refs/tags/v1.1.0.tar.gz
    URL_HASH SHA256=88ece79f3de5ccaec4191951a222f95cc80c4381dafd3163bdb1ff87cedf3118)

set(MPARK_VARIANT_INCLUDE_TESTS "" CACHE STRING "" FORCE)
FetchContent_Declare(mpark_variant
    URL https://github.com/mpark/variant/archive/refs/tags/v1.4.0.tar.gz
    URL_HASH SHA256=8f6b28ab3640b5d76d5b6664dda7257a4405ce59179220431b8fd196c79b2ecb
)

set(_dependencies_parent_folder "${CMAKE_FOLDER}")
set(CMAKE_FOLDER "Dependencies")
# Dependencies keep their own warning policy; project targets retain ours.
get_directory_property(_project_compile_options COMPILE_OPTIONS)
set(_dependency_compile_options ${_project_compile_options})
list(REMOVE_ITEM _dependency_compile_options -Wall -Wextra -pedantic -Werror /W4 /WX)
set_property(DIRECTORY PROPERTY COMPILE_OPTIONS "${_dependency_compile_options}")
FetchContent_MakeAvailable(tl_expected tl_optional mpark_variant googletest)
set_property(DIRECTORY PROPERTY COMPILE_OPTIONS "${_project_compile_options}")
# An empty parent must unset CMAKE_FOLDER, not create an empty IDE group.
set(CMAKE_FOLDER ${_dependencies_parent_folder})
foreach(_dependency tl_expected tl_optional mpark_variant googletest)
    set_property(DIRECTORY "${${_dependency}_SOURCE_DIR}"
        PROPERTY EXCLUDE_FROM_ALL TRUE)
endforeach()
if(NOT TARGET GTest::GTest)
    add_library(GTest::GTest ALIAS gtest)
endif()

if(DEFINED omnirefl_DIR)
    # Keep strict project warnings out of third-party FetchContent targets.
    include("${omnirefl_DIR}/clean_build.cmake")
endif()

if(NOT TARGET std_compat)
    add_library(std_compat INTERFACE)
    target_link_libraries(std_compat INTERFACE
        tl::expected
        tl::optional
        mpark_variant)
endif()
