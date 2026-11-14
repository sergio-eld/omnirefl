# Exercise the patch helper shipped with the selected source or installed package.
if(DEFINED omnirefl_DIR)
    set(_serialization_patch_script
        "${omnirefl_DIR}/components/serialization/patch_c4core_span.cmake")
else()
    set(_serialization_patch_script
        "${omnirefl_SOURCE_DIR}/extensions/serialization/patch_c4core_span.cmake")
endif()
add_test(NAME omnirefl.serialization.patch_reapplication
    COMMAND "${CMAKE_COMMAND}"
        "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/patch-test"
        "-DPATCH_SCRIPT=${_serialization_patch_script}"
        -P "${CMAKE_CURRENT_LIST_DIR}/apply_patch.cmake")

if(NOT TARGET json_model)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../model" json_model)
endif()

foreach(_standard IN LISTS _supported_cpp_standards)
    set(_test_target "cpp_${_standard}.extensions.serialization")
    add_executable(${_test_target} test.cpp scalar_roots.cpp sequence_roots.cpp
        json_mapping.cpp alias_mapping.cpp to_record.cpp to_enum.cpp to_range.cpp
        string_mapping.cpp
        container_discovery.cpp optional_mapping.cpp customization.cpp
        diagnostics.cpp)
    set_target_properties(${_test_target} PROPERTIES
        CXX_STANDARD ${_standard}
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)
    # Exercise serialization and parser recovery without exceptions.
    # FIXME: [external] tl::optional 1.1.0 unconditionally throws in value().
    # Under -fno-exceptions, Clang 6 rejects these bodies even when unused.
    # Omit this test-only flag on Clang 6 until upstream guards the throws;
    # all test cases remain enabled, and no flags propagate to consumers.
    # https://github.com/TartanLlama/optional/issues/56
    if("GNU" STREQUAL CMAKE_CXX_COMPILER_FRONTEND_VARIANT
        AND NOT ("Clang" STREQUAL CMAKE_CXX_COMPILER_ID
            AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 6
            AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7))
        target_compile_options(${_test_target} PRIVATE -fno-exceptions)
    endif()
    target_link_libraries(${_test_target} PRIVATE
        gtest_main omni::serialization json_model)
    omni_reflected_target(${_test_target})
    if(CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME)
        add_test(NAME ${_test_target} COMMAND ${_test_target})
    endif()

endforeach()

# These targets compile only when CTest requests the expected failure.
foreach(_standard IN LISTS _supported_cpp_standards)
    set(_negative_cases missing_mapping)
    if(_standard GREATER_EQUAL 14)
        list(APPEND _negative_cases conflicting_mapping)
    endif()
    foreach(_case IN LISTS _negative_cases)
        set(_target "cpp_${_standard}.negative.serialization.${_case}")
        add_executable(${_target} EXCLUDE_FROM_ALL "negative/${_case}.cpp")
        set_target_properties(${_target} PROPERTIES
            CXX_STANDARD ${_standard}
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO)
        target_link_libraries(${_target} PRIVATE omni::serialization)
        omni_reflected_target(${_target})
        if(_case STREQUAL "missing_mapping")
            set(_message "No node mapping: specialize")
        else()
            set(_message "Conflicting node mappings: specialize either")
        endif()
        add_test(NAME "${_target}.build"
            COMMAND "${CMAKE_COMMAND}"
                "-DBUILD_DIR=${CMAKE_BINARY_DIR}"
                "-DTARGET=${_target}"
                "-DEXPECTED_MESSAGE=${_message}"
                -P "${omnirefl_PROJECTS_DIR}/tests/tool/expect_compile_failure.cmake")
        # A build must not race another CTest build in the same configured tree.
        set_tests_properties("${_target}.build" PROPERTIES RUN_SERIAL TRUE)
    endforeach()
endforeach()
