if(NOT TARGET json_model)
    add_subdirectory("${omnirefl_PROJECTS_DIR}/extensions/serialization/model"
        json_model)
endif()

foreach(_standard IN LISTS _supported_cpp_standards)
    set(_target "cpp_${_standard}.json_ryml.test.smoke")
    add_executable(${_target} main.cpp scalar_roots.cpp custom_mapping.cpp)
    set_target_properties(${_target} PROPERTIES
        CXX_STANDARD ${_standard}
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)
    target_link_libraries(${_target} PRIVATE
        gtest_main omni::serialization json_model)
    # Match the full suite's no-exceptions coverage and tl::optional exception.
    # FIXME: [external] tl::optional 1.1.0 has unconditional throw expressions.
    # https://github.com/TartanLlama/optional/issues/56
    if("GNU" STREQUAL CMAKE_CXX_COMPILER_FRONTEND_VARIANT
        AND NOT ("Clang" STREQUAL CMAKE_CXX_COMPILER_ID
            AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 6
            AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7))
        target_compile_options(${_target} PRIVATE -fno-exceptions)
    endif()
    target_include_directories(${_target} PRIVATE
        "${omnirefl_PROJECTS_DIR}/extensions/serialization/tests")
    omni_reflected_target(${_target})
    if(CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME)
        add_test(NAME ${_target} COMMAND ${_target})
    endif()
endforeach()
