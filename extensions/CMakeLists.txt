# 3.18.2 is the current project floor for CMake APIs used by the package.
cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

if(BUILD_EXTENSIONS)
    include(dependencies.cmake)
    add_library(extensions_common INTERFACE)
    target_compile_features(extensions_common INTERFACE cxx_std_11)
    target_include_directories(extensions_common INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
    target_link_libraries(extensions_common INTERFACE
        omni::refl tl::expected tl::optional nonstd::string-view-lite)
endif()

add_subdirectory(serialization)

if(ENABLE_PACKAGE)
    install(DIRECTORY include/omnirefl/
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/omnirefl")
    install(FILES dependencies.cmake patch_string_view_lite.cmake
        string_view_lite-compare.patch
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/omnirefl/components")
endif()

# TODO: Consider CLI and OpenAPI/REST mapping components.
