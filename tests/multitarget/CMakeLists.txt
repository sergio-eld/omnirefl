get_filename_component(DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(TEST_NAME test.${DIR_NAME})

add_library(${TEST_NAME}.a STATIC impl_a.cpp)
set_target_properties(${TEST_NAME}.a PROPERTIES CXX_STANDARD 11)
reflected_target(${TEST_NAME}.a)

add_library(${TEST_NAME}.b STATIC impl_b.cpp)
set_target_properties(${TEST_NAME}.b PROPERTIES CXX_STANDARD 11)
reflected_target(${TEST_NAME}.b)

add_executable(${TEST_NAME} test.cpp)
target_link_libraries(${TEST_NAME} PRIVATE ${TEST_NAME}.a ${TEST_NAME}.b GTest::gtest)
# todo: investigate the behavior
# reflected_target(${TEST_NAME})

# fixme:
# add_test(NAME ${TEST_NAME}.run_omni
#     COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} -t ${TEST_NAME}.omni)
add_test(NAME ${TEST_NAME}.build
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} -t ${TEST_NAME})
add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})

# set_tests_properties(${TEST_NAME}.build PROPERTIES DEPENDS ${TEST_NAME}.run_omni)
set_tests_properties(${TEST_NAME} PROPERTIES DEPENDS ${TEST_NAME}.build)
