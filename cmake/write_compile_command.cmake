cmake_policy(SET CMP0054 NEW)

foreach(required CCDB_QUERY COMP_DB SOURCE OUTPUT_CONTAINS OUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

execute_process(
    COMMAND "${CCDB_QUERY}" "${COMP_DB}" "${SOURCE}" "${OUTPUT_CONTAINS}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE command
    ERROR_VARIABLE error)
if(NOT 0 EQUAL result)
    message(FATAL_ERROR "ccdb_query failed with code ${result}\n${error}")
endif()

# Leave the timestamp unchanged when only another database entry changed.
if(EXISTS "${OUT}")
    file(READ "${OUT}" previous)
    if("${previous}" STREQUAL "${command}")
        # Ad hoc: response-file contents are absent from the command snapshot.
        # FIXME: [high] Track explicit response files, including nested ones,
        # as dependencies. Until then, preserve conservative regeneration for
        # @file commands when the database changes, even if their text matches.
        separate_arguments(arguments UNIX_COMMAND "${command}")
        list(FILTER arguments INCLUDE REGEX "^@")
        if(NOT arguments)
            return()
        endif()
    endif()
endif()
file(WRITE "${OUT}" "${command}")
