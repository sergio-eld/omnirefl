foreach(required FIXTURE ROOT GENERATOR CXX_COMPILER OMNIREFL_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

set(source_dir "${ROOT}/dollar$path/source")
set(build_dir "${ROOT}/dollar$path/build")
file(REMOVE_RECURSE "${ROOT}/dollar$path")
file(MAKE_DIRECTORY "${source_dir}")
file(COPY "${FIXTURE}/CMakeLists.txt" "${FIXTURE}/main.cpp"
    DESTINATION "${source_dir}")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${source_dir}"
        -B "${build_dir}"
        -G "${GENERATOR}"
        "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
        "-Domnirefl_DIR=${OMNIREFL_DIR}"
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_out
    ERROR_VARIABLE configure_err)
if(NOT 0 EQUAL configure_result)
    message(FATAL_ERROR
        "failed to configure dollar-path fixture\n"
        "stdout:\n${configure_out}\n"
        "stderr:\n${configure_err}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${build_dir}"
        --target dollar_path.omni
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_out
    ERROR_VARIABLE build_err)
if(NOT 0 EQUAL build_result)
    message(FATAL_ERROR
        "failed to instrument dollar-path fixture\n"
        "stdout:\n${build_out}\n"
        "stderr:\n${build_err}")
endif()

string(CONCAT output "${build_out}" "${build_err}")
if(output MATCHES "compiler source after `--` differs")
    message(FATAL_ERROR
        "omnirefl emitted a false source mismatch warning\n${output}")
endif()
