foreach(required CCDB_QUERY COMPILE_DB SOURCE)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

execute_process(
    COMMAND "${CCDB_QUERY}" "${COMPILE_DB}" "${SOURCE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
if(NOT 0 EQUAL result)
    message(FATAL_ERROR "ccdb_query failed\n${error}")
endif()

string(STRIP "${output}" output)
separate_arguments(arguments UNIX_COMMAND "${output}")
list(FIND arguments [=[-DCCDB_BACKSLASH=R"(left\right)"]=] found)
if(-1 EQUAL found)
    message(FATAL_ERROR
        "literal backslash argument was not preserved\n${arguments}")
endif()
