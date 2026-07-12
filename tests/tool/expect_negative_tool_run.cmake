if(NOT DEFINED BUILD_DIR)
    message(FATAL_ERROR "BUILD_DIR is required")
endif()

if(NOT DEFINED TARGET)
    message(FATAL_ERROR "TARGET is required")
endif()

if(NOT DEFINED EXPECTED_DIAGNOSTICS)
    set(EXPECTED_DIAGNOSTICS 1)
endif()

if(NOT DEFINED EXPECTED_MESSAGE)
    set(EXPECTED_MESSAGE "invalid reflection queries")
endif()

if(NOT DEFINED DIAGNOSTIC_REGEX)
    set(DIAGNOSTIC_REGEX ":[0-9]+:[0-9]+: reflection query")
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

if(DEFINED EXPECTED_MESSAGE_2 AND NOT output MATCHES "${EXPECTED_MESSAGE_2}")
    message(FATAL_ERROR
        "Expected '${TARGET}' to report '${EXPECTED_MESSAGE_2}'.\n${output}")
endif()

if(DEFINED UNEXPECTED_MESSAGE
        AND NOT "" STREQUAL "${UNEXPECTED_MESSAGE}"
        AND output MATCHES "${UNEXPECTED_MESSAGE}")
    message(FATAL_ERROR
        "Expected '${TARGET}' not to report '${UNEXPECTED_MESSAGE}'.\n${output}")
endif()

if(NOT output MATCHES "\\^")
    message(FATAL_ERROR
        "Expected '${TARGET}' to render a source caret.\n${output}")
endif()

if(NOT output MATCHES "Suggestion:")
    message(FATAL_ERROR
        "Expected '${TARGET}' to render a suggestion.\n${output}")
endif()

string(REGEX MATCHALL "${DIAGNOSTIC_REGEX}" diagnostics "${output}")
list(LENGTH diagnostics count)

if(NOT EXPECTED_DIAGNOSTICS EQUAL count)
    message(FATAL_ERROR
        "Expected ${EXPECTED_DIAGNOSTICS} diagnostics for '${TARGET}', got ${count}.\n${output}")
endif()

message(STATUS
    "Observed ${count} invalid reflection query diagnostics for '${TARGET}'")
