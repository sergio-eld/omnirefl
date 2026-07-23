add_executable(std_compat_test std_compat_test.cpp)
set_target_properties(std_compat_test PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)
target_include_directories(std_compat_test PRIVATE
    "${CMAKE_SOURCE_DIR}")
target_link_libraries(std_compat_test PRIVATE
    GTest::GTest
    gtest_main)

add_test(NAME omnirefl.std_compat
    COMMAND std_compat_test)
