foreach(required
        FIXTURE
        ROOT
        GENERATOR
        C_COMPILER
        CXX_COMPILER
        OMNIREFL_DIR
        CROSSCOMPILING
        EXECUTABLES)
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
        "-DCMAKE_C_COMPILER=${C_COMPILER}"
        "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
        "-Domnirefl_DIR=${OMNIREFL_DIR}"
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_out
    ERROR_VARIABLE configure_err)
if(NOT 0 EQUAL configure_result)
    message(FATAL_ERROR
        "failed to configure integration project\n"
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

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${ROOT}" --parallel
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_out
    ERROR_VARIABLE build_err)
if(NOT 0 EQUAL build_result)
    message(FATAL_ERROR
        "failed to build integration project\n"
        "stdout:\n${build_out}\n"
        "stderr:\n${build_err}")
endif()

file(GLOB_RECURSE reflection_headers "${ROOT}/*.omnirefl.hpp")
foreach(stem IN LISTS OMITTED_REFLECTION_STEMS)
    if(reflection_headers MATCHES "/${stem}_[^/]*\\.omnirefl\\.hpp")
        message(FATAL_ERROR
            "generated source `${stem}` was unexpectedly reflected:\n"
            "${reflection_headers}")
    endif()
endforeach()

foreach(stem IN LISTS REQUIRED_REFLECTION_STEMS)
    if(NOT reflection_headers MATCHES "/${stem}_[^/]*\\.omnirefl\\.hpp")
        message(FATAL_ERROR
            "source `${stem}` was not reflected:\n"
            "${reflection_headers}")
    endif()
endforeach()

if(NOT CROSSCOMPILING)
    foreach(executable IN LISTS EXECUTABLES)
        execute_process(
            COMMAND "${ROOT}/${executable}${EXECUTABLE_SUFFIX}"
            RESULT_VARIABLE run_result)
        if(NOT 0 EQUAL run_result)
            message(FATAL_ERROR
                "${executable} exited with code ${run_result}")
        endif()
    endforeach()
endif()
