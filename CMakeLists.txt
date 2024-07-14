cmake_minimum_required(VERSION 3.14.2 FATAL_ERROR)

project(clang_sandbox LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Clang REQUIRED)
find_package(LLVM REQUIRED)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

add_executable(refl main.cpp)
set_property(TARGET refl PROPERTY CXX_STANDARD 20)
target_include_directories(refl PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
target_compile_definitions(refl PRIVATE ${LLVM_DEFINITIONS})
target_link_libraries(refl PRIVATE clangTooling)
