foreach(_standard IN LISTS _supported_cpp_standards)
    set(_target "cpp_${_standard}.functional")
    add_executable(${_target} main.cpp compat_pair.cpp ../smoke/functional.cpp)
    set_target_properties(${_target} PROPERTIES
        CXX_STANDARD ${_standard}
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)
    target_link_libraries(${_target} PRIVATE
        omni::refl
        GTest::GTest
        gtest_main)
    omni_reflected_target(${_target})
    # This source verifies the core utility without instrumentation or a
    # forced reflection header. omni_reflected_target snapshots earlier sources.
    target_sources(${_target} PRIVATE lookup_table.cpp)

    if(CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME)
        add_test(NAME "${_target}"
            COMMAND ${_target})
    endif()
endforeach()
