cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl 
    LANGUAGES CXX C 
    VERSION 0.0.1)

# -- Configurable options --------
option(ENABLE_PACKAGE "Enable omnirefl packaging" OFF)
message(STATUS "ENABLE_PACKAGE: ${ENABLE_PACKAGE}")

option(ENABLE_TESTING "Enable testing" OFF)
message(STATUS "ENABLE_TESTING: ${ENABLE_TESTING}")

option(ENABLE_BENCH "Enable benchmarks" OFF)
message(STATUS "ENABLE_BENCH: ${ENABLE_BENCH}")

set(OMNIREFL_PACKAGE_COMMIT "" CACHE STRING "Commit id suffix for package file names")


# -- Scripts --------
include(cmake/clean_build.cmake)
include(cmake/target_link_static_crt.cmake)

function(add_static_compile_options TARGET)
    target_link_static_crt(${TARGET})

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${TARGET} PRIVATE
            -fno-rtti
            -fvisibility=hidden
            -fdata-sections
            -ffunction-sections)

        target_link_options(${TARGET} PRIVATE -Wl,--gc-sections)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(${TARGET} PRIVATE /GR- /GL)
        target_link_options(${TARGET} PRIVATE /LTCG)
    endif()
endfunction()


# -- Project-wide compiler options --------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# -- Dependencies / toolchain introspection --------
include(FetchContent)

# CLI11
FetchContent_Declare(
    cli11_proj
    QUIET
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.6.1
)
FetchContent_MakeAvailable(cli11_proj)

# LLVM / Clang
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
find_package(Clang REQUIRED)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "CLANG_INCLUDE_DIRS: ${CLANG_INCLUDE_DIRS}")
message(STATUS "LLVM_INCLUDE_DIRS: ${LLVM_INCLUDE_DIRS}")

unset(CLANG_RESOURCE_DIR CACHE)
foreach(dir IN LISTS CLANG_INCLUDE_DIRS)
    message(STATUS "Looking for Clang resource dir in: ${dir}/../")
    file(GLOB CLANG_RESOURCE_DIR_CANDIDATE "${dir}/../lib/clang/*/include")
    if(CLANG_RESOURCE_DIR_CANDIDATE)
        list(GET CLANG_RESOURCE_DIR_CANDIDATE 0 CLANG_RESOURCE_DIR)
        get_filename_component( CLANG_RESOURCE_DIR "${CLANG_RESOURCE_DIR}/../" ABSOLUTE)
        message(STATUS "CLANG_RESOURCE_DIR: ${CLANG_RESOURCE_DIR}")
        break()
    endif()
endforeach()

if(NOT CLANG_RESOURCE_DIR)
    message(FATAL_ERROR "Failed to detect Clang resource dir")
endif()


# -- Tool executable --------
add_executable(tool main.cpp)
add_executable(omni::tool ALIAS tool)

set_target_properties(tool PROPERTIES
    OUTPUT_NAME ${PROJECT_NAME}
    INTERPROCEDURAL_OPTIMIZATION ON
    ENABLE_EXPORTS OFF)

target_include_directories(tool PRIVATE
    ${CLANG_INCLUDE_DIRS}
    ${LLVM_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR})

target_link_libraries(tool PRIVATE CLI11::CLI11 clangTooling)
add_static_compile_options(tool)

add_executable(ccdb_query ccdb_query.cpp)
add_executable(omni::ccdb_query ALIAS ccdb_query)

set_target_properties(ccdb_query PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION ON
    ENABLE_EXPORTS OFF)

target_include_directories(ccdb_query SYSTEM PRIVATE
    ${CLANG_INCLUDE_DIRS}
    ${LLVM_INCLUDE_DIRS})

target_include_directories(ccdb_query PRIVATE
    ${PROJECT_SOURCE_DIR})

target_link_libraries(ccdb_query PRIVATE CLI11::CLI11 clangTooling)
add_static_compile_options(ccdb_query)

# -- Tool frontend --------
add_library(refl INTERFACE)
add_library(omni::refl ALIAS refl)

target_sources(refl INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}/reflection.hpp>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}/reflection.hpp>

    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}/unique_id.hpp>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}/unique_id.hpp>)

target_include_directories(refl INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# -- Testing / packaging setup --------
if(NOT CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME)
    if(ENABLE_TESTING)
        message(STATUS
            "Testing during build is disabled for cross compilation "
            "(host: ${CMAKE_HOST_SYSTEM_NAME}, target: ${CMAKE_SYSTEM_NAME})"
        )
    endif()
elseif(ENABLE_TESTING OR ENABLE_PACKAGE OR ENABLE_BENCH)
    enable_testing()

    # Tests in the subdir are packaged and expect to work with `find_package(omnirefl)`.
    # These lines emulate the 'found' package.
    set(${PROJECT_NAME}_RESOURCE_DIR "${CLANG_RESOURCE_DIR}")
    include(cmake/reflected_target.cmake)

    add_subdirectory(tests/tool tests)

    add_subdirectory(tests/ccdb_query tests/ccdb_query)
endif()

if(ENABLE_PACKAGE)
    message(STATUS "Generating ${PROJECT_NAME} package")
    include(GNUInstallDirs)

    install(TARGETS tool ccdb_query refl
        EXPORT "${PROJECT_NAME}Targets"
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    install(DIRECTORY
        "${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}")

    # Bundle Clang builtin headers for the tool.
    install(DIRECTORY
        ${CLANG_RESOURCE_DIR}/include
        DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/clang/${LLVM_VERSION}")

    # Ship tests as source so they can be run on other platforms.
    install(DIRECTORY
        ${CMAKE_SOURCE_DIR}/tests/tool/
        DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/tests")

    install(EXPORT "${PROJECT_NAME}Targets"
        FILE "${PROJECT_NAME}Targets.cmake"
        NAMESPACE omni::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

    include(CMakePackageConfigHelpers)

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY AnyNewerVersion ARCH_INDEPENDENT)

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/reflected_target.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_omnirefl_from_ccdb.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

    # fixme: do not print message when building for windows
    # Debian architecture: required because Alpine has no dpkg to auto-detect.
    if(NOT CPACK_DEBIAN_PACKAGE_ARCHITECTURE)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
            set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
            set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7" OR
               CMAKE_SYSTEM_PROCESSOR MATCHES "armv7l")
            set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "armhf")
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv6")
            set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "armel")
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
            set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
        else()
            message(WARNING
                "Unknown Debian architecture for ${CMAKE_SYSTEM_PROCESSOR}; "
                "defaulting to amd64. Override CPACK_DEBIAN_PACKAGE_ARCHITECTURE "
                "if this is wrong.")
            set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
        endif()
    endif()

    set(CPACK_PACKAGE_CONTACT "sergey.kolesnik.ld@gmail.com")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Sergei Kolesnik")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS NO)

    string(TOLOWER "${CMAKE_SYSTEM_NAME}" OMNIREFL_PACKAGE_OS)
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" OMNIREFL_PACKAGE_ARCH)
    if(OMNIREFL_PACKAGE_OS STREQUAL "" OR OMNIREFL_PACKAGE_ARCH STREQUAL "")
        message(FATAL_ERROR
            "Package target requires CMAKE_SYSTEM_NAME and CMAKE_SYSTEM_PROCESSOR")
    endif()

    if(OMNIREFL_PACKAGE_ARCH MATCHES "^(x86_64|amd64)$")
        set(OMNIREFL_PACKAGE_ARCH "x86_64")
    elseif(OMNIREFL_PACKAGE_ARCH MATCHES "^(aarch64|arm64)$")
        set(OMNIREFL_PACKAGE_ARCH "aarch64")
    elseif(OMNIREFL_PACKAGE_ARCH MATCHES "^i[3-6]86$")
        set(OMNIREFL_PACKAGE_ARCH "x86")
    endif()

    set(OMNIREFL_PACKAGE_TARGET "${OMNIREFL_PACKAGE_OS}-${OMNIREFL_PACKAGE_ARCH}")
    set(CPACK_PACKAGE_FILE_NAME
        "${PROJECT_NAME}-${OMNIREFL_PACKAGE_TARGET}-${PROJECT_VERSION}")

    if(NOT OMNIREFL_PACKAGE_COMMIT STREQUAL "")
        set(CPACK_PACKAGE_FILE_NAME
            "${CPACK_PACKAGE_FILE_NAME}-${OMNIREFL_PACKAGE_COMMIT}")
    endif()

    set(CPACK_DEBIAN_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}.deb")

    include(CPack)
endif()
