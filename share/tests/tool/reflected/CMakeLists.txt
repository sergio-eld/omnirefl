set(_not_reflected_message
    "omni::reflected: type was not reflected")

function(_reflected_compile_failure cpp_standard name)
    set(_target "cpp_${cpp_standard}.negative.reflected.${name}")

    add_executable(${_target} EXCLUDE_FROM_ALL "negative/${name}.cpp")
    set_target_properties(${_target} PROPERTIES
        CXX_STANDARD ${cpp_standard}
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )
    target_link_libraries(${_target} PRIVATE std_compat)
    omni_reflected_target(${_target} ENABLE_INDEX_MODE)

    add_test(NAME "${_target}.build"
        COMMAND ${CMAKE_COMMAND}
            -D "BUILD_DIR=${CMAKE_BINARY_DIR}"
            -D "TARGET=${_target}"
            -D "EXPECTED_MESSAGE=${_not_reflected_message}"
            -P "${CMAKE_CURRENT_LIST_DIR}/../expect_compile_failure.cmake")

    # FIXME: Enable once reflected(...) reports one focused unavailable-type
    # diagnostic without cascading errors from incomplete generated metadata.
    set_tests_properties("${_target}.build" PROPERTIES DISABLED TRUE)
endfunction()

foreach(_cpp_standard IN LISTS _supported_cpp_standards)
    _reflected_compile_failure(${_cpp_standard} unreflected_type)
    _reflected_compile_failure(${_cpp_standard} unreflected_value)
endforeach()
