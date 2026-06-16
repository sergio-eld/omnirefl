if(NOT DEFINED CCDB_QUERY)
    message(FATAL_ERROR "CCDB_QUERY is required")
endif()
if(NOT DEFINED OMNIREFL)
    message(FATAL_ERROR "OMNIREFL is required")
endif()
if(NOT DEFINED COMP_DB)
    message(FATAL_ERROR "COMP_DB is required")
endif()
if(NOT DEFINED SOURCE)
    message(FATAL_ERROR "SOURCE is required")
endif()
if(NOT DEFINED OUTPUT_CONTAINS)
    message(FATAL_ERROR "OUTPUT_CONTAINS is required")
endif()
if(NOT DEFINED OUT)
    message(FATAL_ERROR "OUT is required")
endif()
if(NOT DEFINED RESOURCE_DIR)
    message(FATAL_ERROR "RESOURCE_DIR is required")
endif()
if(NOT DEFINED NO_ANNOTATIONS)
    set(NO_ANNOTATIONS 0)
endif()

execute_process(
    COMMAND "${CCDB_QUERY}" "${COMP_DB}" "${SOURCE}" "${OUTPUT_CONTAINS}"
    RESULT_VARIABLE _ccdb_result
    OUTPUT_VARIABLE _compiler_command
    ERROR_VARIABLE _ccdb_err)

if(NOT 0 EQUAL _ccdb_result)
    message(FATAL_ERROR "${_ccdb_err}")
endif()

string(STRIP "${_compiler_command}" _compiler_command)
separate_arguments(_compiler_args UNIX_COMMAND "${_compiler_command}")

set(_omnirefl_args
    "${OMNIREFL}"
    --resource-dir "${RESOURCE_DIR}"
    -o "${OUT}"
    --source "${SOURCE}")

if(NO_ANNOTATIONS)
    list(APPEND _omnirefl_args --no-annotations)
endif()

list(APPEND _omnirefl_args -- ${_compiler_args})

execute_process(
    COMMAND ${_omnirefl_args}
    RESULT_VARIABLE _omnirefl_result
    OUTPUT_VARIABLE _omnirefl_out
    ERROR_VARIABLE _omnirefl_err)

if(_omnirefl_out)
    message("${_omnirefl_out}")
endif()
if(_omnirefl_err)
    message("${_omnirefl_err}")
endif()

if(NOT 0 EQUAL _omnirefl_result)
    message(FATAL_ERROR
        "omnirefl failed with code ${_omnirefl_result}"
        "\nstdout:\n${_omnirefl_out}"
        "\nstderr:\n${_omnirefl_err}")
endif()
