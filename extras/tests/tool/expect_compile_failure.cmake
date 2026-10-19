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
    COMMAND "${CMAKE_COMMAND}" --build "${BUILD_DIR}" -t "${TARGET}" --parallel 1
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
)

string(CONCAT output "${out}" "${err}")

if(0 EQUAL rc)
    message(FATAL_ERROR "Expected '${TARGET}' compilation to fail")
endif()

if(NOT output MATCHES "${EXPECTED_MESSAGE}")
    message(FATAL_ERROR
        "Expected '${TARGET}' to report '${EXPECTED_MESSAGE}'.\n${output}")
endif()

if(DEFINED UNEXPECTED_MESSAGE AND output MATCHES "${UNEXPECTED_MESSAGE}")
    message(FATAL_ERROR
        "Expected '${TARGET}' not to report '${UNEXPECTED_MESSAGE}'.\n${output}")
endif()

message(STATUS "Observed expected '${TARGET}' compilation failure")
