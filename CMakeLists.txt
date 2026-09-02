# 3.18.2 is the current project floor for CMake APIs used by the package and
# reflected target integration.
cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl 
    LANGUAGES CXX C 
    VERSION 0.0.3)

# -- Configurable options --------
option(ENABLE_PACKAGE "Enable omnirefl packaging" OFF)
message(STATUS "ENABLE_PACKAGE: ${ENABLE_PACKAGE}")

option(ENABLE_TESTING "Enable testing" OFF)
message(STATUS "ENABLE_TESTING: ${ENABLE_TESTING}")

option(ENABLE_EXTENSIONS "Enable extensions in the dev build" OFF)
message(STATUS "ENABLE_EXTENSIONS: ${ENABLE_EXTENSIONS}")

option(ENABLE_BENCH "Enable benchmarks" OFF)
message(STATUS "ENABLE_BENCH: ${ENABLE_BENCH}")

set(OMNIREFL_PACKAGE_COMMIT "" CACHE STRING "Commit id suffix for package file names")
set(OMNIREFL_PACKAGE_TARGET "" CACHE STRING "Override the platform component of package file names")
set(CLANG_RESOURCE_DIR "" CACHE PATH "Clang resource directory containing builtin headers")

# ad hoc: when using the MinGW GCC cross compiler directly, CMake may leave
# CMAKE_SYSTEM_PROCESSOR empty unless configured through a toolchain file.
# Infer the target processor from the compiler triplet so package metadata and
# downstream project logic see a consistent target architecture.
if("" STREQUAL CMAKE_SYSTEM_PROCESSOR AND
   CMAKE_CXX_COMPILER MATCHES "x86_64-w64-mingw32")
    set(CMAKE_SYSTEM_PROCESSOR "x86_64")
endif()


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

if(CLANG_RESOURCE_DIR)
    message(STATUS "Using Clang resource dir: ${CLANG_RESOURCE_DIR}")
else()
    foreach(dir IN LISTS CLANG_INCLUDE_DIRS)
        message(STATUS "Looking for Clang resource dir in: ${dir}/../")
        file(GLOB CLANG_RESOURCE_DIR_CANDIDATE "${dir}/../lib/clang/*/include")
        if(CLANG_RESOURCE_DIR_CANDIDATE)
            list(GET CLANG_RESOURCE_DIR_CANDIDATE 0 CLANG_RESOURCE_DIR)
            get_filename_component(CLANG_RESOURCE_DIR "${CLANG_RESOURCE_DIR}/../" ABSOLUTE)
            message(STATUS "CLANG_RESOURCE_DIR: ${CLANG_RESOURCE_DIR}")
            break()
        endif()
    endforeach()
endif()

if(NOT CLANG_RESOURCE_DIR)
    message(FATAL_ERROR "Failed to detect Clang resource dir")
endif()


# -- Tool executable --------
set(_omnirefl_enable_ipo ON)
if(COSMOPOLITAN_DETECTED)
    # The released cosmocc GCC binaries do not include an LTO plugin.
    set(_omnirefl_enable_ipo OFF)
endif()

add_executable(tool main.cpp)
add_executable(omni::tool ALIAS tool)

set_target_properties(tool PROPERTIES
    OUTPUT_NAME ${PROJECT_NAME}
    INTERPROCEDURAL_OPTIMIZATION ${_omnirefl_enable_ipo}
    ENABLE_EXPORTS OFF)

target_include_directories(tool PRIVATE
    ${CLANG_INCLUDE_DIRS}
    ${LLVM_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include)

target_compile_definitions(tool PRIVATE
    OMNIREFL_VERSION="${PROJECT_VERSION}"
    OMNIREFL_COMMIT="${OMNIREFL_PACKAGE_COMMIT}")

target_link_libraries(tool PRIVATE CLI11::CLI11 clangTooling)
add_static_compile_options(tool)

add_executable(ccdb_query ccdb_query.cpp)
add_executable(omni::ccdb_query ALIAS ccdb_query)

set_target_properties(ccdb_query PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION ${_omnirefl_enable_ipo}
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

    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}/functional.hpp>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}/functional.hpp>

    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}/traits.hpp>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}/traits.hpp>

    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}/unique_id.hpp>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}/unique_id.hpp>)

target_include_directories(refl INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

if(ENABLE_EXTENSIONS)
    # Public extension implementations. Component install rules place their
    # headers and CMake package files under include/ and lib/.
    add_subdirectory(extensions)

    # Tests, examples, sandboxes, and benchmarks for extensions. The same
    # project is installed under share/ for installed-package validation.
    set(omnirefl_RESOURCE_DIR "${CLANG_RESOURCE_DIR}")
    include(cmake/reflected_target.cmake)
    set(_in_omnirefl_build ON)
    add_subdirectory(extensions_dev)
endif()

# -- Testing / packaging setup --------
if(NOT CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME)
    if(ENABLE_TESTING)
        message(STATUS
            "Testing during build is disabled for cross compilation "
            "(host: ${CMAKE_HOST_SYSTEM_NAME}, target: ${CMAKE_SYSTEM_NAME})"
        )
    endif()
elseif(ENABLE_BENCH OR ENABLE_TESTING)
    enable_testing()

    # tests/tool is packaged and expects to work with `find_package(omnirefl)`.
    # These lines emulate the 'found' package.
    set(${PROJECT_NAME}_RESOURCE_DIR "${CLANG_RESOURCE_DIR}")
    include(cmake/reflected_target.cmake)

    add_subdirectory(tests/tool tests)
    add_subdirectory(tests/ccdb_query tests/ccdb_query)
    if(ENABLE_TESTING)
        add_subdirectory(tests/std_compat tests/std_compat)
        add_subdirectory(tests/functional tests/functional)
    endif()
endif()

if(ENABLE_PACKAGE)
    message(STATUS "Generating ${PROJECT_NAME} package")
    include(GNUInstallDirs)

    set(_package_license_dir
        "${CMAKE_CURRENT_BINARY_DIR}/package-licenses/${LLVM_PACKAGE_VERSION}")
    file(MAKE_DIRECTORY "${_package_license_dir}")

    function(_omnirefl_package_license name url)
        set(_license_path "${_package_license_dir}/${name}")
        file(DOWNLOAD
            "${url}"
            "${_license_path}"
            STATUS _status
            TIMEOUT 30
            TLS_VERIFY ON)
        list(GET _status 0 _status_code)
        list(GET _status 1 _status_message)

        if(0 EQUAL _status_code)
            file(SIZE "${_license_path}" _license_size)
            if(0 LESS _license_size)
                install(FILES "${_license_path}"
                    DESTINATION "${CMAKE_INSTALL_DOCDIR}/licenses")
                return()
            endif()

            set(_status_message "downloaded an empty file")
        endif()

        file(REMOVE "${_license_path}")
        message(WARNING
            "Package will omit ${name}: ${_status_message}")
    endfunction()

    # LLVM and Clang installations expose their version through find_package,
    # but do not reliably install or export their project license paths.
    set(_llvm_license_base
        "https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-${LLVM_PACKAGE_VERSION}")
    _omnirefl_package_license(
        "LLVM.txt" "${_llvm_license_base}/llvm/LICENSE.TXT")
    _omnirefl_package_license(
        "Clang.txt" "${_llvm_license_base}/clang/LICENSE.TXT")
    _omnirefl_package_license(
        "libcxx.txt" "${_llvm_license_base}/libcxx/LICENSE.TXT")
    _omnirefl_package_license(
        "libcxxabi.txt" "${_llvm_license_base}/libcxxabi/LICENSE.TXT")

    # target_link_static_crt() sets this property when it embeds libgcc or
    # libstdc++ into a packaged executable.
    get_target_property(_tool_static_gcc_runtime
        tool OMNIREFL_STATIC_GCC_RUNTIME)
    get_target_property(_ccdb_query_static_gcc_runtime
        ccdb_query OMNIREFL_STATIC_GCC_RUNTIME)
    if(_tool_static_gcc_runtime OR _ccdb_query_static_gcc_runtime)
        # Static libgcc/libstdc++ code is covered by GPLv3 as modified by the
        # GCC Runtime Library Exception, so a package embedding it needs both.
        set(_gcc_license_base
            "https://gcc.gnu.org/git/?p=gcc.git&a=blob_plain&hb=releases/gcc-${CMAKE_CXX_COMPILER_VERSION}")
        _omnirefl_package_license(
            "GCC-GPL-3.0.txt" "${_gcc_license_base}&f=COPYING3")
        _omnirefl_package_license(
            "GCC-Runtime-Library-Exception.txt"
            "${_gcc_license_base}&f=COPYING.RUNTIME")
    endif()

    # target_link_static_crt() sets this property when it applies a fully
    # static musl link to a packaged executable.
    get_target_property(_tool_static_musl tool OMNIREFL_STATIC_MUSL)
    get_target_property(_ccdb_query_static_musl
        ccdb_query OMNIREFL_STATIC_MUSL)
    if(_tool_static_musl OR _ccdb_query_static_musl)
        execute_process(
            COMMAND ldd --version
            OUTPUT_VARIABLE _musl_version_output
            ERROR_VARIABLE _musl_version_error)
        string(REGEX MATCH
            "Version[ \t]+([0-9]+\\.[0-9]+\\.[0-9]+)"
            _musl_version_match
            "${_musl_version_output}\n${_musl_version_error}")
        if("" STREQUAL _musl_version_match)
            message(WARNING
                "Package will omit musl.txt: failed to detect musl version")
        else()
            _omnirefl_package_license(
                "musl.txt"
                "https://raw.githubusercontent.com/ifduyue/musl/v${CMAKE_MATCH_1}/COPYRIGHT")
        endif()
    endif()

    get_target_property(_tool_static_cosmopolitan tool OMNIREFL_STATIC_COSMOPOLITAN)
    get_target_property(_ccdb_query_static_cosmopolitan ccdb_query OMNIREFL_STATIC_COSMOPOLITAN)
    if(_tool_static_cosmopolitan OR _ccdb_query_static_cosmopolitan)
        get_filename_component(_cosmopolitan_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
        get_filename_component(_cosmopolitan_root "${_cosmopolitan_bin_dir}" DIRECTORY)

        foreach(_license gpl2 gpl3 lgpl2 lgpl3)
            set(_license_path "${_cosmopolitan_root}/LICENSE.${_license}")
            if(EXISTS "${_license_path}")
                install(FILES "${_license_path}"
                    DESTINATION "${CMAKE_INSTALL_DOCDIR}/licenses"
                    RENAME "Cosmopolitan-${_license}.txt")
            else()
                message(WARNING
                    "Package will omit Cosmopolitan ${_license} notice: "
                    "${_license_path} does not exist")
            endif()
        endforeach()
    endif()

    if(MINGW)
        set(_mingw_version_source
            "${CMAKE_CURRENT_BINARY_DIR}/mingw-version.c")
        file(WRITE "${_mingw_version_source}" "#include <_mingw.h>\n")
        execute_process(
            COMMAND "${CMAKE_C_COMPILER}" -dM -E "${_mingw_version_source}"
            RESULT_VARIABLE _mingw_version_result
            OUTPUT_VARIABLE _mingw_version_defines
            ERROR_VARIABLE _mingw_version_error)
        if(NOT 0 EQUAL _mingw_version_result)
            message(WARNING
                "Package will omit MinGW notices: ${_mingw_version_error}")
        else()
            set(_mingw_version_components)
            foreach(_component MAJOR MINOR BUGFIX)
                string(REGEX MATCH
                    "#define __MINGW64_VERSION_${_component}[ \t]+([0-9]+)"
                    _component_match
                    "${_mingw_version_defines}")
                if("" STREQUAL _component_match)
                    break()
                endif()

                list(APPEND _mingw_version_components "${CMAKE_MATCH_1}")
            endforeach()

            list(LENGTH _mingw_version_components _mingw_component_count)
            if(3 EQUAL _mingw_component_count)
                list(JOIN _mingw_version_components "." _mingw_version)
                set(_mingw_license_base
                    "https://raw.githubusercontent.com/mingw-w64/mingw-w64/v${_mingw_version}")
                _omnirefl_package_license(
                    "MinGW-w64-runtime.txt"
                    "${_mingw_license_base}/COPYING.MinGW-w64-runtime/COPYING.MinGW-w64-runtime.txt")
                _omnirefl_package_license(
                    "winpthreads.txt"
                    "${_mingw_license_base}/mingw-w64-libraries/winpthreads/COPYING")
            else()
                message(WARNING
                    "Package will omit MinGW notices: failed to detect "
                    "MinGW-w64 version")
            endif()
        endif()
    endif()

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

    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        DESTINATION "${CMAKE_INSTALL_DOCDIR}")

    install(FILES
        "${cli11_proj_SOURCE_DIR}/LICENSE"
        DESTINATION "${CMAKE_INSTALL_DOCDIR}/licenses"
        RENAME "CLI11.txt")

    # Ship tests and examples as source so they can be used on other platforms.
    install(DIRECTORY
        ${CMAKE_SOURCE_DIR}/tests/tool/
        DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/tests")

    install(DIRECTORY
        ${CMAKE_SOURCE_DIR}/tests/functional/
        DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/tests/functional")

    install(DIRECTORY
        ${CMAKE_SOURCE_DIR}/examples/
        DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/examples")

    install(DIRECTORY
        ${CMAKE_SOURCE_DIR}/extensions_dev/
        DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/examples/extensions")

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
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/clean_build.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/reflected_target.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_omnirefl_from_ccdb.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

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

    if(OMNIREFL_PACKAGE_TARGET STREQUAL "")
        string(TOLOWER "${CMAKE_SYSTEM_NAME}" OMNIREFL_PACKAGE_OS)
        string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" OMNIREFL_PACKAGE_ARCH)
        if(OMNIREFL_PACKAGE_OS STREQUAL "" OR OMNIREFL_PACKAGE_ARCH STREQUAL "")
            message(FATAL_ERROR "Package target requires CMAKE_SYSTEM_NAME and CMAKE_SYSTEM_PROCESSOR")
        endif()

        if(OMNIREFL_PACKAGE_ARCH MATCHES "^(x86_64|amd64)$")
            set(OMNIREFL_PACKAGE_ARCH "x86_64")
        elseif(OMNIREFL_PACKAGE_ARCH MATCHES "^(aarch64|arm64)$")
            set(OMNIREFL_PACKAGE_ARCH "aarch64")
        elseif(OMNIREFL_PACKAGE_ARCH MATCHES "^i[3-6]86$")
            set(OMNIREFL_PACKAGE_ARCH "x86")
        endif()

        set(OMNIREFL_PACKAGE_TARGET "${OMNIREFL_PACKAGE_OS}-${OMNIREFL_PACKAGE_ARCH}")
        if(OMNIREFL_PACKAGE_OS STREQUAL "linux")
            set(OMNIREFL_PACKAGE_TARGET "${OMNIREFL_PACKAGE_TARGET}-musl")
        elseif(OMNIREFL_PACKAGE_OS STREQUAL "windows")
            set(OMNIREFL_PACKAGE_TARGET "${OMNIREFL_PACKAGE_TARGET}-ucrt")
        endif()
    endif()

    set(CPACK_PACKAGE_FILE_NAME
        "${PROJECT_NAME}-${OMNIREFL_PACKAGE_TARGET}-${PROJECT_VERSION}")

    if(NOT OMNIREFL_PACKAGE_COMMIT STREQUAL "")
        set(CPACK_PACKAGE_FILE_NAME
            "${CPACK_PACKAGE_FILE_NAME}-${OMNIREFL_PACKAGE_COMMIT}")
    endif()

    set(CPACK_DEBIAN_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}.deb")

    include(CPack)
endif()
