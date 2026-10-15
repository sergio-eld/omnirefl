include(FetchContent)

# Enable FetchContent logging
set(FETCHCONTENT_QUIET OFF CACHE BOOL "" FORCE)

# The tests use Google Test without Google Mock.
set(BUILD_GMOCK OFF CACHE BOOL "Disable unused Google Mock targets" FORCE)
# do not let googletest install itself when run "cmake --install" / CPack
set(INSTALL_GTEST OFF CACHE BOOL "Disable installation of googletest" FORCE)
set(INSTALL_GMOCK OFF CACHE BOOL "Disable installation of googlemock" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
     # The "newest" gtest that supports C++11.
     # Otherwize will force its C++ version upon the test targets.
    GIT_TAG release-1.12.1)

# std-compat utility
set(EXPECTED_BUILD_PACKAGE OFF CACHE BOOL "" FORCE)
set(EXPECTED_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(tl_expected
    GIT_REPOSITORY https://github.com/TartanLlama/expected.git
    GIT_TAG v1.3.1
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE)

set(OPTIONAL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(tl_optional
    GIT_REPOSITORY https://github.com/TartanLlama/optional.git
    GIT_TAG v1.1.0
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE)

set(MPARK_VARIANT_INCLUDE_TESTS "" CACHE STRING "" FORCE)
FetchContent_Declare(mpark_variant
    GIT_REPOSITORY https://github.com/mpark/variant.git
    GIT_TAG v1.4.0
    GIT_SHALLOW TRUE
    GIT_SUBMODULES ""
    GIT_SUBMODULES_RECURSE FALSE
)

FetchContent_MakeAvailable(tl_expected tl_optional mpark_variant googletest)
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
