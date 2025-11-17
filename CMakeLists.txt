cmake_minimum_required(VERSION 3.14.2 FATAL_ERROR)

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
            -ffunction-sections
        )
        target_link_options(${TARGET} PRIVATE
            -Wl,--gc-sections
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(${TARGET} PRIVATE
            /GR-
            /GL
        )
        target_link_options(${TARGET} PRIVATE
            /LTCG
        )
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
    GIT_TAG v2.3.2
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
    file(GLOB CLANG_RESOURCE_DIR_CANDIDATE
        "${dir}/../lib/clang/*/include"
    )
    if(CLANG_RESOURCE_DIR_CANDIDATE)
        list(GET CLANG_RESOURCE_DIR_CANDIDATE 0 CLANG_RESOURCE_DIR)
        get_filename_component(
            CLANG_RESOURCE_DIR
            "${CLANG_RESOURCE_DIR}/../"
            ABSOLUTE
        )
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
    ENABLE_EXPORTS OFF
)

target_include_directories(tool PRIVATE
    ${CLANG_INCLUDE_DIRS}
    ${LLVM_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(tool PRIVATE CLI11::CLI11 clangTooling)
add_static_compile_options(tool)


# -- Tool frontend --------
add_library(refl INTERFACE)
add_library(omni::refl ALIAS refl)

target_sources(refl INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}/refl.hpp>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}/refl.hpp>
)

target_include_directories(refl INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)


# -- Testing / packaging setup --------
if(ENABLE_TESTING OR ENABLE_PACKAGE)
    enable_testing()

    # Tests in the subdir are packaged and expect to work with `find_package(omnirefl)`.
    # These lines emulate the 'found' package.
    set(${PROJECT_NAME}_RESOURCE_DIR "${CLANG_RESOURCE_DIR}")
    include(cmake/reflected_target.cmake)

    # add_subdirectory(tests) #< fixme: uncomment when tests are fixed
endif()

if(ENABLE_PACKAGE)
    message(STATUS "Generating ${PROJECT_NAME} package")
    include(GNUInstallDirs)

    install(TARGETS tool refl
        EXPORT "${PROJECT_NAME}Targets"
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}/refl.hpp
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}"
    )

    # Bundle Clang builtin headers for the tool.
    install(DIRECTORY
        ${CLANG_RESOURCE_DIR}/include
        DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/clang/${LLVM_VERSION}"
    )

    # Ship tests as source so they can be run on other platforms.
    install(DIRECTORY
        ${CMAKE_SOURCE_DIR}/tests
        DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}"
    )

    install(EXPORT "${PROJECT_NAME}Targets"
        FILE "${PROJECT_NAME}Targets.cmake"
        NAMESPACE omni::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
    )

    include(CMakePackageConfigHelpers)

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY AnyNewerVersion
    )

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/reflected_target.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
    )

    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_PACKAGE_CONTACT "sergey.kolesnik.ld@gmail.com")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Sergei Kolesnik")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

    # Fully-static build on Alpine: no shared libs to scan for dependencies.
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS NO)

    include(CPack)
endif()
