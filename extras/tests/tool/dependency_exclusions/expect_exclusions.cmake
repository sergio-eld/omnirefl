execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --target "${TARGET}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
if(NOT 0 EQUAL rc)
    message(FATAL_ERROR "Exclusion test build failed:\n${out}\n${err}")
endif()

if("${out}${err}" MATCHES
        "overloaded public member function '[^'\r\n]*dependency_exclusions::opaque::")
    message(FATAL_ERROR "Skipped metadata must not produce member warnings:\n${out}\n${err}")
endif()

if("${out}${err}" MATCHES
        "reflection dependency '[^'\r\n]*exclusion_protocol::virtual_wrapper[^'\r\n]*' skipped: records with virtual bases")
    message(FATAL_ERROR "Metadata restrictions must not block skipped discovery routes:\n${out}\n${err}")
endif()

file(GLOB headers "${HEADER_DIR}/*.omnirefl.hpp")
list(LENGTH headers count)
if(NOT 1 EQUAL count)
    message(FATAL_ERROR "Expected one generated header, found: ${headers}")
endif()
file(READ "${headers}" header)
string(REGEX MATCHALL
    "// metadata skip requested at [^\r\n]*${EXCLUSION_FILE}:[0-9]+:[0-9]+: [^\r\n]*"
    annotations "${header}")
if(NOT annotations)
    message(FATAL_ERROR "Generated header does not identify the exclusion location")
endif()

set(reported_types)
foreach(annotation IN LISTS annotations)
    string(REGEX REPLACE "^// metadata skip requested at .*:[0-9]+:[0-9]+: " ""
        types "${annotation}")
    list(APPEND reported_types "${types}")

    # Incremental builds reuse the header; fresh generation must log the same report.
    if("${out}${err}" MATCHES "generating reflection")
        string(SUBSTRING "${annotation}" 3 -1 report)
        string(FIND "${out}${err}" "${report}" position)
        if(-1 EQUAL position)
            message(FATAL_ERROR "Missing exclusion log report: ${report}\n${out}\n${err}")
        endif()
    endif()
endforeach()
foreach(expected IN LISTS EXPECTED_TYPES)
    list(FIND reported_types "${expected}" position)
    if(-1 EQUAL position)
        message(FATAL_ERROR
            "Missing exclusion type list: ${expected}\nReported: ${reported_types}")
    endif()
endforeach()
