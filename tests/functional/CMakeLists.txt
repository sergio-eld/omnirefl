if(NOT COMMAND _omnirefl_supported_cpp_standards)
    include("${CMAKE_CURRENT_LIST_DIR}/../tool/supported_cpp_standards.cmake")
endif()
_omnirefl_supported_cpp_standards(_supported_cpp_standards)

foreach(_standard IN LISTS _supported_cpp_standards)
    set(_target "cpp_${_standard}.functional")
    add_executable(${_target} test.cpp)
    set_target_properties(${_target} PROPERTIES
        CXX_STANDARD ${_standard}
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)
    target_link_libraries(${_target} PRIVATE
        omni::refl
        GTest::GTest
        gtest_main)
    omni_reflected_target(${_target})

    if(CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME)
        add_test(NAME "${_target}"
            COMMAND ${_target})
    endif()
endforeach()
