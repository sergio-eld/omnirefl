foreach(required FIXTURE ROOT GENERATOR CXX_COMPILER OMNIREFL_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

# Script generation must preserve spaces and literal dollar signs.
set(build_dir "${ROOT}/with space/dollar$path")
file(REMOVE_RECURSE "${ROOT}")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${FIXTURE}"
        -B "${build_dir}"
        -G "${GENERATOR}"
        "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
        "-Domnirefl_DIR=${OMNIREFL_DIR}"
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_out
    ERROR_VARIABLE configure_err)
if(NOT 0 EQUAL configure_result)
    message(FATAL_ERROR
        "Force-regenerate project configuration failed:\n"
        "${configure_out}\n${configure_err}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${build_dir}"
        --target many.omni --parallel 1
    RESULT_VARIABLE initial_result
    OUTPUT_VARIABLE initial_out
    ERROR_VARIABLE initial_err)
if(NOT 0 EQUAL initial_result)
    message(FATAL_ERROR
        "Initial generation failed:\n${initial_out}\n${initial_err}")
endif()

file(GLOB reflection_headers "${build_dir}/omni_many/*.omnirefl.hpp")
foreach(header IN LISTS reflection_headers)
    file(WRITE "${header}" "#error Reflection must be regenerated\n")
endforeach()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${build_dir}"
        --target many.omni --parallel 1
    RESULT_VARIABLE regenerate_result
    OUTPUT_VARIABLE regenerate_out
    ERROR_VARIABLE regenerate_err)
if(NOT 0 EQUAL regenerate_result)
    message(FATAL_ERROR
        "Force regeneration failed:\n${regenerate_out}\n${regenerate_err}")
endif()

# Compile every source to check that force regeneration replaced every header.
execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${build_dir}" --parallel 1
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_out
    ERROR_VARIABLE build_err)
if(NOT 0 EQUAL build_result)
    message(FATAL_ERROR
        "Build after force regeneration failed:\n${build_out}\n${build_err}")
endif()
