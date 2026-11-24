# Standalone, opt-in experiment; never configured or built by CI.
# A: existing generated metadata and base collection.
# B: flat metadata with inherited field definitions copied into each record.
# C: flat metadata tuples reusing field definitions from their declaring bases.
cmake_minimum_required(VERSION 3.25)
project(omnirefl_accessible_fields LANGUAGES CXX)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(omnirefl CONFIG REQUIRED COMPONENTS serialization)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
if(OMNI_BENCH_TOOL)
    get_target_property(configurations omni::tool IMPORTED_CONFIGURATIONS)
    foreach(configuration IN LISTS configurations)
        set_property(TARGET omni::tool PROPERTY
            "IMPORTED_LOCATION_${configuration}" "${OMNI_BENCH_TOOL}")
    endforeach()
endif()

set(OMNI_CASES "bases4_20_read" CACHE STRING "Record-shape cases to compare")
get_filename_component(repo "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(generated "${CMAKE_CURRENT_BINARY_DIR}/sources")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare.py"
        "${generated}" ${OMNI_CASES}
    COMMAND_ERROR_IS_FATAL ANY)

# B/C bypass the C++ collection machinery and consume the emitted flat tuple.
# All other headers, compile options and source contents are identical.
file(READ "${repo}/include/omnirefl/reflected_scope.hpp" scope)
string(REPLACE
    "using accessible_data_members_t = detail::_all_visible_public_fields_t<_M>;"
    "using accessible_data_members_t = typename _M::accessible_public_fields_t;"
    flat_scope "${scope}")
if(flat_scope STREQUAL scope)
    message(FATAL_ERROR "Accessible-fields alias changed; update the experiment")
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/flat/omnirefl/reflected_scope.hpp" "${flat_scope}")

foreach(case IN LISTS OMNI_CASES)
    foreach(layout IN ITEMS A B C)
        set(target "${layout}_${case}")
        add_executable(${target} EXCLUDE_FROM_ALL "${generated}/${case}.${layout}.cpp")
        target_include_directories(${target} BEFORE PRIVATE
            "${repo}/include"
            "${repo}/extensions/include"
            "${repo}/extensions/serialization/include")
        target_link_libraries(${target} PRIVATE omni::serialization)
        set_target_properties(${target} PROPERTIES CXX_COMPILER_LAUNCHER
            "${Python3_EXECUTABLE};${CMAKE_CURRENT_SOURCE_DIR}/measure.py;${CMAKE_BINARY_DIR}/stats;${target}")
    endforeach()

    omni_reflected_target("A_${case}" LOG_LEVEL warning)
    # Get the actual generated header without guessing its filename.
    string(SHA1 source_hash "${generated}/${case}.A.cpp")
    get_target_property(original "A_${case}" "OMNIREFL_FORCE_INCLUDE_${source_hash}")
    set(copied "${CMAKE_CURRENT_BINARY_DIR}/${case}.B.hpp")
    set(referenced "${CMAKE_CURRENT_BINARY_DIR}/${case}.C.hpp")
    add_custom_command(OUTPUT "${copied}" "${referenced}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/layout.py"
            "${original}" "${copied}" "${referenced}"
        DEPENDS "${original}" "${CMAKE_CURRENT_SOURCE_DIR}/layout.py"
        VERBATIM)
    add_custom_target("layouts_${case}" DEPENDS "${copied}" "${referenced}")
    add_dependencies("layouts_${case}" "_gen.A_${case}.omni")

    foreach(layout IN ITEMS B C)
        set(header "${copied}")
        if(layout STREQUAL C)
            set(header "${referenced}")
        endif()
        add_dependencies("${layout}_${case}" "layouts_${case}")
        target_include_directories("${layout}_${case}" BEFORE PRIVATE
            "${CMAKE_CURRENT_BINARY_DIR}/flat")
        if(MSVC)
            target_compile_options("${layout}_${case}" PRIVATE "/FI${header}")
        else()
            target_compile_options("${layout}_${case}" PRIVATE -include "${header}")
        endif()
    endforeach()
endforeach()
