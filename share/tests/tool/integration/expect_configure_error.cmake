foreach(required
        FIXTURE
        ROOT
        GENERATOR
        CXX_COMPILER
        OMNIREFL_DIR
        EXPECTED_DIAGNOSTICS)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${ROOT}")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${FIXTURE}"
        -B "${ROOT}"
        -G "${GENERATOR}"
        "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
        "-Domnirefl_DIR=${OMNIREFL_DIR}"
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_out
    ERROR_VARIABLE configure_err)

if(0 EQUAL configure_result)
    message(FATAL_ERROR
        "integration project unexpectedly configured successfully\n"
        "stdout:\n${configure_out}\n"
        "stderr:\n${configure_err}")
endif()

string(CONCAT configure_log "${configure_out}\n" "${configure_err}")
foreach(expected IN LISTS EXPECTED_DIAGNOSTICS)
    string(FIND "${configure_log}" "${expected}" expected_position)
    if(-1 EQUAL expected_position)
        message(FATAL_ERROR
            "missing expected diagnostic `${expected}`\n"
            "configure output:\n${configure_log}")
    endif()
endforeach()
