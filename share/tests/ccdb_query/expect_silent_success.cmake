execute_process(
    COMMAND "${CCDB_QUERY}" "${COMPILE_DB}" "${SOURCE}" "${OUTPUT_CONTAINS}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err)

if(NOT _result EQUAL 0)
    message(FATAL_ERROR "ccdb_query failed: ${_result}\n${_err}")
endif()
if(NOT _out MATCHES "${EXPECT_OUTPUT}")
    message(FATAL_ERROR "Expected `${EXPECT_OUTPUT}` in stdout:\n${_out}")
endif()
if(NOT _err STREQUAL "")
    message(FATAL_ERROR "Valid commands produced diagnostics:\n${_err}")
endif()
