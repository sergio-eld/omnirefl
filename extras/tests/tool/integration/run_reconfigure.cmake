set(EXECUTABLES "")
set(REQUIRED_REFLECTION_STEMS first)
include("${CMAKE_CURRENT_LIST_DIR}/run_project.cmake")

function(snapshot target prefix)
    file(GLOB headers "${ROOT}/omni_${target}/*.omnirefl.hpp")
    list(LENGTH headers count)
    if(NOT 1 EQUAL count)
        message(FATAL_ERROR "Expected one reflection header for ${target}: ${headers}")
    endif()
    list(GET headers 0 header)
    file(READ "${ROOT}/${target}.path" library)
    foreach(artifact header library)
        file(TIMESTAMP "${${artifact}}" timestamp UTC)
        if(NOT timestamp)
            message(FATAL_ERROR "Missing ${artifact}: ${${artifact}}")
        endif()
        set(${prefix}_${target}_${artifact} "${timestamp}" PARENT_SCOPE)
    endforeach()
endfunction()

function(expect_unchanged target prefix)
    snapshot(${target} current)
    foreach(artifact header library)
        if(NOT "${${prefix}_${target}_${artifact}}" STREQUAL
                "${current_${target}_${artifact}}")
            message(FATAL_ERROR "Unrelated change rebuilt ${target}'s ${artifact}")
        endif()
    endforeach()
endfunction()

function(build)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${ROOT}" --parallel
        RESULT_VARIABLE result
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err)
    if(NOT 0 EQUAL result)
        message(FATAL_ERROR "Build failed:\n${out}\n${err}")
    endif()
endfunction()

function(reconfigure option)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-D${option}=ON" "${ROOT}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err)
    if(NOT 0 EQUAL result)
        message(FATAL_ERROR "Reconfiguration failed:\n${out}\n${err}")
    endif()
endfunction()

snapshot(first initial)
snapshot(second initial)
# Portable CMake timestamps have second precision; separate each mutation.
execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 1)
file(READ "${ROOT}/compile_commands.json" database)
file(WRITE "${ROOT}/compile_commands.json" "${database}")
build()
expect_unchanged(first initial)
expect_unchanged(second initial)

execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 1)
reconfigure(SECOND_EXTRA_DEFINITION)
build()
expect_unchanged(first initial)
snapshot(second changed)
foreach(artifact header library)
    if("${initial_second_${artifact}}" STREQUAL "${changed_second_${artifact}}")
        message(FATAL_ERROR "Changed compile command did not rebuild second's ${artifact}")
    endif()
endforeach()

execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 1)
reconfigure(WITH_THIRD)
build()
snapshot(third added)
expect_unchanged(first initial)
expect_unchanged(second changed)

execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 1)
build()
expect_unchanged(first initial)
expect_unchanged(second changed)
expect_unchanged(third added)

execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 1)
reconfigure(THIRD_EXTRA_DEFINITION)
build()
expect_unchanged(first initial)
expect_unchanged(second changed)
snapshot(third response_changed)
foreach(artifact header library)
    if("${added_third_${artifact}}" STREQUAL "${response_changed_third_${artifact}}")
        message(FATAL_ERROR "Changed response file did not rebuild third's ${artifact}")
    endif()
endforeach()
