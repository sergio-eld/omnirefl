cmake_minimum_required(VERSION 3.26.4 FATAL_ERROR)

project(sample LANGUAGES CXX)

add_library(o1 STATIC o1.cpp)
add_library(o2 STATIC o2.cpp)

add_executable(e1 e1.cpp)
# add_library(e1 STATIC e1.cpp) # for when e1 fails to be built
reflected_target(e1) # todo: fix the tool
target_link_libraries(e1 PRIVATE omni::refl) # todo: remove after fixing
target_link_libraries(e1 PRIVATE o1 o2)
