# 3.18.2 is the current project floor for CMake APIs used by the package.
cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

add_subdirectory(serialization)

if(ENABLE_PACKAGE)
    install(DIRECTORY include/omnirefl/
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/omnirefl")
endif()

# TODO: Consider CLI and OpenAPI/REST mapping components.
