foreach(required
        CCDB_QUERY
        OMNIREFL
        RESOURCE_DIR
        CXX_COMPILER
        MSVC_RESPONSE_FILE
        ROOT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${ROOT}")
file(MAKE_DIRECTORY
    "${ROOT}/db"
    "${ROOT}/command/include"
    "${ROOT}/command/src")
file(WRITE "${ROOT}/command/include/config.h"
    "#define OMNI_COMPILE_DB_DIRECTORY 1\n")
file(WRITE "${ROOT}/command/src/main.cpp"
    "#include <config.h>\n"
    "#if !defined(OMNI_COMPILE_DB_DIRECTORY)\n"
    "#  error \"compile-command directory was not preserved\"\n"
    "#endif\n"
    "#if !defined(OMNI_RESPONSE_FILE_EXPANDED)\n"
    "#  error \"compiler response file was not expanded\"\n"
    "#endif\n"
    "int compile_db_directory_control;\n")

if(MSVC_RESPONSE_FILE)
    set(compile_source "/c")
    set(include_option "/Iinclude")
    set(output_option "/Fo")
    file(WRITE "${ROOT}/command/frontend.rsp"
        "/DOMNI_RESPONSE_FILE_EXPANDED\n"
        [=["/DOMNI_RESPONSE_TEXT=\"left right\""]=] "\n")
else()
    set(compile_source "-c")
    set(include_option "-Iinclude")
    set(output_option "-o")
    file(WRITE "${ROOT}/command/frontend.rsp"
        "-DOMNI_RESPONSE_FILE_EXPANDED\n"
        [=["-DOMNI_RESPONSE_TEXT=\"left right\""]=] "\n")
endif()

file(TO_CMAKE_PATH "${CXX_COMPILER}" compiler)
file(TO_CMAKE_PATH "${ROOT}/command" command_directory)
file(TO_CMAKE_PATH "${ROOT}/command/src/main.cpp" source)
file(WRITE "${ROOT}/db/compile_commands.json"
    "[{\n"
    "  \"directory\": \"${command_directory}\",\n"
    "  \"arguments\": [\"${compiler}\", \"${compile_source}\", "
        "\"@frontend.rsp\", \"${include_option}\", \"src/main.cpp\", "
        "\"${output_option}\", \"main.o\"],\n"
    "  \"file\": \"${source}\",\n"
    "  \"output\": \"main.o\"\n"
    "}]\n")

execute_process(
    COMMAND "${CCDB_QUERY}"
        "${ROOT}/db/compile_commands.json"
        "${ROOT}/command/src/main.cpp"
    WORKING_DIRECTORY "${ROOT}/db"
    RESULT_VARIABLE query_result
    OUTPUT_VARIABLE compiler_command
    ERROR_VARIABLE query_error)
if(NOT 0 EQUAL query_result)
    message(FATAL_ERROR "ccdb_query failed:\n${query_error}")
endif()

if(NOT compiler_command MATCHES "-working-directory")
    message(FATAL_ERROR
        "compile command does not preserve its working directory:\n"
        "${compiler_command}")
endif()

string(FIND "${compiler_command}" "${command_directory}" directory_position)
if(-1 EQUAL directory_position)
    message(FATAL_ERROR
        "compile command has the wrong working directory:\n${compiler_command}")
endif()

string(STRIP "${compiler_command}" compiler_command)
separate_arguments(compiler_args UNIX_COMMAND "${compiler_command}")

execute_process(
    COMMAND "${OMNIREFL}"
        --resource-dir "${RESOURCE_DIR}"
        --log-level silent
        -o "${ROOT}/main.omnirefl.hpp"
        -c "${ROOT}/command/src/main.cpp"
        -- ${compiler_args}
    WORKING_DIRECTORY "${ROOT}/db"
    RESULT_VARIABLE tool_result
    OUTPUT_VARIABLE tool_out
    ERROR_VARIABLE tool_error)
if(NOT 0 EQUAL tool_result)
    message(FATAL_ERROR
        "omnirefl failed for the compile-command directory:\n"
        "stdout:\n${tool_out}\n"
        "stderr:\n${tool_error}")
endif()
