if(NOT "17" IN_LIST _supported_cpp_standards)
    return()
endif()

function(_aggregate_into_compile_failure name expected_message unexpected_message)
    set(_target "cpp_17.negative.aggregate_into.${name}")

    add_executable(${_target} EXCLUDE_FROM_ALL "${name}.cpp")
    set_target_properties(${_target} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )
    target_compile_definitions(${_target} PRIVATE CXX_STANDARD=17)
    target_link_libraries(${_target} PRIVATE std_compat)
    omni_reflected_target(${_target} ENABLE_INDEX_MODE)

    add_test(NAME "${_target}.build"
        COMMAND ${CMAKE_COMMAND}
            -D "BUILD_DIR=${CMAKE_BINARY_DIR}"
            -D "TARGET=${_target}"
            -D "EXPECTED_MESSAGE=${expected_message}"
            -D "UNEXPECTED_MESSAGE=${unexpected_message}"
            -P "${CMAKE_CURRENT_LIST_DIR}/expect_compile_failure.cmake")
endfunction()

set(_missing_message
    "omni::refl::aggregate_into: destination field is missing from the source fields")
set(_incompatible_message
    "omni::refl::aggregate_into: destination field is not constructible from the same-named source field")
set(_unsupported_message
    "omni::refl::aggregate_into: destination type is not a supported aggregate")

_aggregate_into_compile_failure(
    missing_field
    "${_missing_message}"
    "${_incompatible_message}")
_aggregate_into_compile_failure(
    missing_after_optional_completion
    "${_missing_message}"
    "${_incompatible_message}")
_aggregate_into_compile_failure(
    compatible_type_wrong_name
    "${_missing_message}"
    "${_incompatible_message}")
_aggregate_into_compile_failure(
    incompatible_field
    "${_incompatible_message}"
    "${_missing_message}")
_aggregate_into_compile_failure(
    nested_field
    "${_incompatible_message}"
    "${_missing_message}")
_aggregate_into_compile_failure(
    unsupported_destination
    "${_unsupported_message}"
    "destination field")
