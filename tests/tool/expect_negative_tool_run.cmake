if(NOT DEFINED BUILD_DIR)
    message(FATAL_ERROR "BUILD_DIR is required")
endif()

if(NOT DEFINED TARGET)
    message(FATAL_ERROR "TARGET is required")
endif()

if(NOT DEFINED MIN_DIAGNOSTICS)
    set(MIN_DIAGNOSTICS 1)
endif()

if(NOT DEFINED EXPECTED_MESSAGE)
    set(EXPECTED_MESSAGE "reflection query instantiated during the tool run")
endif()

if(NOT DEFINED DIAGNOSTIC_REGEX)
    set(DIAGNOSTIC_REGEX ":[0-9]+:[0-9]+: omni::")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${BUILD_DIR}" -t "${TARGET}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
)

string(CONCAT output "${out}" "${err}")

if(0 EQUAL rc)
    message(FATAL_ERROR "Expected '${TARGET}' tool run to fail")
endif()

if(NOT output MATCHES "${EXPECTED_MESSAGE}")
    message(FATAL_ERROR
        "Expected '${TARGET}' to report '${EXPECTED_MESSAGE}'.\n${output}")
endif()

string(REGEX MATCHALL "${DIAGNOSTIC_REGEX}" diagnostics "${output}")
list(LENGTH diagnostics count)

if(count LESS MIN_DIAGNOSTICS)
    message(FATAL_ERROR
        "Expected at least ${MIN_DIAGNOSTICS} diagnostics for '${TARGET}', got ${count}.\n${output}")
endif()

message(STATUS
    "Observed ${count} invalid reflection query diagnostics for '${TARGET}'")
