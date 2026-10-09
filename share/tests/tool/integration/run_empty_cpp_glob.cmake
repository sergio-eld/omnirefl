foreach(required
        FIXTURE
        ROOT
        GENERATOR
        C_COMPILER
        CXX_COMPILER
        OMNIREFL_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${ROOT}")
set(source_dir "${ROOT}/source")
set(build_dir "${ROOT}/build")
file(MAKE_DIRECTORY "${source_dir}")
file(COPY "${FIXTURE}/" DESTINATION "${source_dir}")
file(MAKE_DIRECTORY "${source_dir}/optional")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${source_dir}"
        -B "${build_dir}"
        -G "${GENERATOR}"
        "-DCMAKE_C_COMPILER=${C_COMPILER}"
        "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
        "-Domnirefl_DIR=${OMNIREFL_DIR}"
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_out
    ERROR_VARIABLE configure_err)
if(NOT 0 EQUAL configure_result)
    message(FATAL_ERROR
        "failed to configure empty-glob project\n"
        "stdout:\n${configure_out}\n"
        "stderr:\n${configure_err}")
endif()

string(CONCAT configure_log "${configure_out}\n" "${configure_err}")
foreach(expected "no C++ translation units" "skipped.")
    string(FIND "${configure_log}" "${expected}" expected_position)
    if(-1 EQUAL expected_position)
        message(FATAL_ERROR
            "missing expected diagnostic `${expected}`\n"
            "configure output:\n${configure_log}")
    endif()
endforeach()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${build_dir}" --parallel
    RESULT_VARIABLE initial_build_result
    OUTPUT_VARIABLE initial_build_out
    ERROR_VARIABLE initial_build_err)
if(NOT 0 EQUAL initial_build_result)
    message(FATAL_ERROR
        "failed to build empty-glob project\n"
        "stdout:\n${initial_build_out}\n"
        "stderr:\n${initial_build_err}")
endif()

file(GLOB_RECURSE initial_reflection_headers
    "${build_dir}/omni_*/*.omnirefl.hpp")
if(initial_reflection_headers)
    message(FATAL_ERROR
        "empty glob unexpectedly generated reflection headers:\n"
        "${initial_reflection_headers}")
endif()

# Older Ninja versions need a new filesystem timestamp tick before checking
# whether the configured glob changed.
execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 1)

configure_file(
    "${source_dir}/optional.cpp.in"
    "${source_dir}/optional/reflected.cpp"
    COPYONLY)

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${build_dir}" --parallel
    RESULT_VARIABLE populated_build_result
    OUTPUT_VARIABLE populated_build_out
    ERROR_VARIABLE populated_build_err)
if(NOT 0 EQUAL populated_build_result)
    message(FATAL_ERROR
        "failed to build populated-glob project\n"
        "stdout:\n${populated_build_out}\n"
        "stderr:\n${populated_build_err}")
endif()

file(GLOB_RECURSE populated_reflection_headers
    "${build_dir}/omni_*/*.omnirefl.hpp")
if(NOT populated_reflection_headers MATCHES
        "/reflected_[^/]*\\.omnirefl\\.hpp")
    message(FATAL_ERROR
        "source added to the glob was not reflected:\n"
        "${populated_reflection_headers}")
endif()
