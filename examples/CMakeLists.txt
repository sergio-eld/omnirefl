cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)

project(omnirefl_examples LANGUAGES CXX)

function(_examples_check_cxx23_library result)
    include(CheckCXXSourceCompiles)
    # Keep the probe's language and link settings local to this function.
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    if(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_REQUIRED_LIBRARIES stdc++exp)
    endif()
    check_cxx_source_compiles(
        "#include <format>
        #include <print>
        #include <ranges>
        #include <string>
        #include <string_view>
        int main() {
            static_assert(std::formattable<int, char>);
            const std::string text = std::string_view{\"hello\"}
                | std::ranges::to<std::string>();
            std::println(\"{}\", std::format(\"{}\", text));
        }"
        _examples_has_cxx23_library)
    set(${result} "${_examples_has_cxx23_library}" PARENT_SCOPE)
endfunction()

if(NOT "cxx_std_23" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    message(STATUS "omnirefl: skipping sneak_peek: C++23 is not available")
else()
    _examples_check_cxx23_library(_examples_has_cxx23_library)
    if(_examples_has_cxx23_library)
        add_subdirectory(sneak_peek)
    else()
        message(STATUS "omnirefl: skipping sneak_peek: C++23 library features are not available")
    endif()
endif()

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR OR TARGET omni::serialization)
    add_subdirectory(extensions/serialization/ryml)
else()
    message(STATUS "omnirefl: skipping json_serialization: extensions disabled")
endif()
