foreach(required
        TOOL
        RESOURCE_DIR
        SOURCE
        COMPILER
        OUT
        EXPECT_SUCCESS)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

file(REMOVE "${OUT}")

execute_process(
    COMMAND "${TOOL}"
        --resource-dir "${RESOURCE_DIR}"
        --log-level error
        -o "${OUT}"
        -c "${SOURCE}"
        -- "${COMPILER}" ${LANGUAGE_ARGS}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)

string(CONCAT output "${out}" "${err}")
if(EXPECT_SUCCESS)
    if(NOT 0 EQUAL result OR NOT EXISTS "${OUT}")
        message(FATAL_ERROR
            "expected C++ frontend invocation to succeed\n${output}")
    endif()
else()
    if(0 EQUAL result)
        message(FATAL_ERROR
            "expected C frontend invocation to be rejected\n${output}")
    endif()

    foreach(expected "requires a C++ translation unit" "selects C")
        string(FIND "${output}" "${expected}" expected_position)
        if(-1 EQUAL expected_position)
            message(FATAL_ERROR
                "missing expected diagnostic `${expected}`\n${output}")
        endif()
    endforeach()
endif()
