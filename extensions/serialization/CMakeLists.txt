if(BUILD_EXTENSIONS)
    include(dependencies.cmake)

    add_library(serialization INTERFACE)
    add_library(omni::serialization ALIAS serialization)
    target_compile_features(serialization INTERFACE cxx_std_11)
    target_include_directories(serialization INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
    target_link_libraries(serialization INTERFACE
        extensions_common ryml::ryml)
endif()

if(ENABLE_PACKAGE)
    install(DIRECTORY include/omnirefl/
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/omnirefl")
    install(FILES serialization.cmake
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/omnirefl/components")
    install(FILES dependencies.cmake patch_c4core_span.cmake
        patch_tl_optional.cmake tl_optional-emplace.patch
        DESTINATION
            "${CMAKE_INSTALL_LIBDIR}/cmake/omnirefl/components/serialization")
endif()
