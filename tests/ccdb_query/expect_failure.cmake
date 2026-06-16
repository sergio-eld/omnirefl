if(NOT DEFINED CCDB_QUERY)
    message(FATAL_ERROR "CCDB_QUERY is required")
endif()
if(NOT DEFINED COMPILE_DB)
    message(FATAL_ERROR "COMPILE_DB is required")
endif()
if(NOT DEFINED SOURCE)
    message(FATAL_ERROR "SOURCE is required")
endif()
if(NOT DEFINED EXPECT)
    message(FATAL_ERROR "EXPECT is required")
endif()

set(_cmd "${CCDB_QUERY}" "${COMPILE_DB}" "${SOURCE}")
if(DEFINED OUTPUT_CONTAINS AND NOT OUTPUT_CONTAINS STREQUAL "")
    list(APPEND _cmd "${OUTPUT_CONTAINS}")
endif()

execute_process(
    COMMAND ${_cmd}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err)

if(0 EQUAL _result)
    message(FATAL_ERROR
        "ccdb_query unexpectedly passed\n"
        "stdout:\n${_out}\n"
        "stderr:\n${_err}")
endif()

string(CONCAT _combined "${_out}" "${_err}")
if(NOT _combined MATCHES "${EXPECT}")
    message(FATAL_ERROR
        "ccdb_query error did not match `${EXPECT}`\n"
        "stdout:\n${_out}\n"
        "stderr:\n${_err}")
endif()
