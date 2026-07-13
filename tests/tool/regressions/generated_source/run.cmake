foreach(required FIXTURE ROOT GENERATOR CXX_COMPILER OMNIREFL_DIR)
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
if(NOT 0 EQUAL configure_result)
    message(FATAL_ERROR
        "failed to configure generated-source fixture\n"
        "stdout:\n${configure_out}\n"
        "stderr:\n${configure_err}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${ROOT}"
        --target generated_source
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_out
    ERROR_VARIABLE build_err)
if(NOT 0 EQUAL build_result)
    message(FATAL_ERROR
        "failed to build generated-source fixture\n"
        "stdout:\n${build_out}\n"
        "stderr:\n${build_err}")
endif()

execute_process(
    COMMAND "${ROOT}/generated_source"
    RESULT_VARIABLE run_result)
if(NOT 0 EQUAL run_result)
    message(FATAL_ERROR
        "generated-source fixture exited with code ${run_result}")
endif()
