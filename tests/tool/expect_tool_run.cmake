if(NOT DEFINED BUILD_DIR)
    message(FATAL_ERROR "BUILD_DIR is required")
endif()

if(NOT DEFINED TARGET)
    message(FATAL_ERROR "TARGET is required")
endif()

if(NOT DEFINED EXPECTED_MESSAGE)
    message(FATAL_ERROR "EXPECTED_MESSAGE is required")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${BUILD_DIR}" -t "${TARGET}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
)

string(CONCAT output "${out}" "${err}")

if(NOT 0 EQUAL rc)
    message(FATAL_ERROR "Expected '${TARGET}' tool run to succeed.\n${output}")
endif()

if(NOT output MATCHES "${EXPECTED_MESSAGE}")
    message(FATAL_ERROR
        "Expected '${TARGET}' to report '${EXPECTED_MESSAGE}'.\n${output}")
endif()

if(DEFINED EXPECTED_MESSAGE_2
        AND NOT "" STREQUAL "${EXPECTED_MESSAGE_2}"
        AND NOT output MATCHES "${EXPECTED_MESSAGE_2}")
    message(FATAL_ERROR
        "Expected '${TARGET}' to report '${EXPECTED_MESSAGE_2}'.\n${output}")
endif()

message(STATUS "Observed expected diagnostics for '${TARGET}'")
