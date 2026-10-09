foreach(required
        OMNIREFL
        RESOURCE_DIR
        SOURCE
        CALLER_DIR
        COMPILER_DIR
        OUTPUT
        DRIVER_ARGS)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

set(caller_header "${CALLER_DIR}/${OUTPUT}")
set(caller_dependencies "${caller_header}.d")
set(compiler_header "${COMPILER_DIR}/${OUTPUT}")
set(compiler_dependencies "${compiler_header}.d")
file(REMOVE
    "${caller_header}"
    "${caller_dependencies}"
    "${compiler_header}"
    "${compiler_dependencies}")

execute_process(
    COMMAND "${OMNIREFL}"
        --resource-dir "${RESOURCE_DIR}"
        --log-level silent
        -o "${OUTPUT}"
        -c "${SOURCE}"
        -- ${DRIVER_ARGS}
    WORKING_DIRECTORY "${CALLER_DIR}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr)

if(NOT 0 EQUAL result)
    message(FATAL_ERROR
        "omnirefl exited with code ${result}\n"
        "stdout:\n${stdout}\n"
        "stderr:\n${stderr}")
endif()

foreach(expected "${caller_header}" "${caller_dependencies}")
    if(NOT EXISTS "${expected}")
        message(FATAL_ERROR "expected output was not created: ${expected}")
    endif()
endforeach()

foreach(unexpected "${compiler_header}" "${compiler_dependencies}")
    if(EXISTS "${unexpected}")
        message(FATAL_ERROR
            "output was created in the compiler working directory: "
            "${unexpected}")
    endif()
endforeach()
