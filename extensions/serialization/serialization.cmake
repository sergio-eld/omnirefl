if(TARGET omni::serialization)
    set(omnirefl_serialization_FOUND TRUE)
    return()
endif()

include("${CMAKE_CURRENT_LIST_DIR}/serialization-dependencies.cmake")

foreach(_dependency IN ITEMS ryml::ryml tl::expected tl::optional)
    if(NOT TARGET "${_dependency}")
        set(omnirefl_serialization_FOUND FALSE)
        set(omnirefl_serialization_NOT_FOUND_MESSAGE
            "omnirefl serialization requires dependency target '${_dependency}'")
        return()
    endif()
endforeach()

# Keep third-party targets out of the core export. Requesting this component
# finds or fetches them before defining the installed extension target.
add_library(omni::serialization INTERFACE IMPORTED)
set_target_properties(omni::serialization PROPERTIES
    INTERFACE_COMPILE_FEATURES cxx_std_11
    INTERFACE_INCLUDE_DIRECTORIES "${omnirefl_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES "omni::refl;ryml::ryml;tl::expected;tl::optional")
set(omnirefl_serialization_FOUND TRUE)
