cmake_minimum_required(VERSION 3.26.4 FATAL_ERROR)

project(sample LANGUAGES CXX)

add_library(o1 STATIC o1.cpp)
add_library(o2 STATIC o2.cpp)

# todo: add header, which contains generated type traits
# todo: figure out a robust way to generate the reflection (as custom command)
set(generated_reflection ${CMAKE_CURRENT_BINARY_DIR}/_reflected.cpp) 
set_source_files_properties(${generated_reflection} PROPERTIES GENERATED TRUE)
add_library(refl STATIC ${generated_reflection})

# todo: generate as custom command
add_executable(e1 e1.cpp)
target_link_libraries(e1 PRIVATE o1 o2 refl)

#TODO: custom command to run the generation
