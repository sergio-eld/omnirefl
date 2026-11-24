# Host tools remain runnable when the reflected executable is cross-compiled.
add_test(NAME omnirefl.cli.help COMMAND $<TARGET_FILE:omni::tool> --help)
set_tests_properties(omnirefl.cli.help PROPERTIES
    PASS_REGULAR_EXPRESSION
        "Usage: omnirefl -o <reflection.hpp> -c <source.cpp>")

add_test(NAME ccdb_query.cli.help
    COMMAND $<TARGET_FILE:omni::ccdb_query> --help)
set_tests_properties(ccdb_query.cli.help PROPERTIES
    PASS_REGULAR_EXPRESSION
        "Usage: ccdb_query <compile_commands.json> <source.cpp> \\[output-contains\\]")

add_test(NAME omnirefl.cli.version
    COMMAND $<TARGET_FILE:omni::tool> --version)
set_tests_properties(omnirefl.cli.version PROPERTIES
    PASS_REGULAR_EXPRESSION
        "omnirefl [0-9]+\\.[0-9]+\\.[0-9]+ \\(commit [^,)]+, LLVM [0-9]+\\.[0-9]+\\.[0-9]+\\)")

add_test(NAME ccdb_query.cli.version
    COMMAND $<TARGET_FILE:omni::ccdb_query> --version)
set_tests_properties(ccdb_query.cli.version PROPERTIES
    PASS_REGULAR_EXPRESSION
        "ccdb_query [0-9]+\\.[0-9]+\\.[0-9]+ \\(commit [^)]+\\)")

foreach(_standard IN LISTS _supported_cpp_standards)
    set(_target "cpp_${_standard}.test.smoke")
    add_executable(${_target} main.cpp)
    set_target_properties(${_target} PROPERTIES
        CXX_STANDARD ${_standard}
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)
    target_compile_definitions(${_target} PRIVATE CXX_STANDARD=${_standard})
    target_link_libraries(${_target} PRIVATE omni::refl gtest_main std_compat)
    target_include_directories(${_target} PRIVATE
        "${omnirefl_PROJECTS_DIR}/tests")
    omni_reflected_target(${_target})
    target_sources(${_target} PRIVATE functional.cpp)
    if(CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME)
        add_test(NAME ${_target} COMMAND ${_target})
    endif()
endforeach()
