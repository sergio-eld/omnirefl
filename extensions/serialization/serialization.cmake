if(TARGET omni::serialization)
    set(omnirefl_serialization_FOUND TRUE)
    return()
endif()

# Dependencies may already have been provided through FetchContent.
# Quiet lookup lets OPTIONAL_COMPONENTS leave the base package usable.
if(NOT TARGET ryml::ryml)
    find_package(ryml 0.14 CONFIG QUIET)
endif()
if(NOT TARGET tl::expected)
    find_package(tl-expected CONFIG QUIET)
endif()
if(NOT TARGET tl::optional)
    find_package(tl-optional CONFIG QUIET)
endif()

if(omnirefl_FIND_REQUIRED_serialization
    AND (NOT TARGET ryml::ryml
        OR NOT TARGET tl::expected
        OR NOT TARGET tl::optional))
    include("${CMAKE_CURRENT_LIST_DIR}/serialization/dependencies.cmake")
endif()

foreach(_dependency IN ITEMS ryml::ryml tl::expected tl::optional)
    if(NOT TARGET "${_dependency}")
        set(omnirefl_serialization_FOUND FALSE)
        set(omnirefl_serialization_NOT_FOUND_MESSAGE
            "omnirefl serialization requires dependency target '${_dependency}'")
        return()
    endif()
endforeach()

# Keep third-party targets out of the core export; this opt-in component
# resolves them before defining the installed extension target.
add_library(omni::serialization INTERFACE IMPORTED)
set_target_properties(omni::serialization PROPERTIES
    INTERFACE_COMPILE_FEATURES cxx_std_11
    INTERFACE_INCLUDE_DIRECTORIES "${omnirefl_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES "omni::refl;ryml::ryml;tl::expected;tl::optional")
set(omnirefl_serialization_FOUND TRUE)
