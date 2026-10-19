function(omnirefl_supported_cpp_standards out)
    set(_standards "")
    foreach(_feature IN LISTS CMAKE_CXX_COMPILE_FEATURES)
        if(_feature MATCHES "^cxx_std_([0-9]+)$")
            list(APPEND _standards "${CMAKE_MATCH_1}")
        endif()
    endforeach()

    list(REMOVE_DUPLICATES _standards)
    list(SORT _standards)

    # C++98 predates omnirefl's minimum; C++26 has standard reflection.
    list(REMOVE_ITEM _standards "98" "26")

    # MSVC has no distinct /std:c++11; CMake effectively treats it as C++14.
    # Normalize 11 -> 14 to avoid generating duplicate targets.
    if(MSVC)
        set(_msvc_standards "")
        foreach(_standard IN LISTS _standards)
            if(_standard STREQUAL "11")
                set(_standard "14")
            endif()
            list(APPEND _msvc_standards "${_standard}")
        endforeach()
        set(_standards "${_msvc_standards}")
        list(REMOVE_DUPLICATES _standards)
        list(SORT _standards)
    endif()

    set(${out} "${_standards}" PARENT_SCOPE)
endfunction()

function(omnirefl_check_cxx20_language result)
    set(_omnirefl_has_cxx20_language OFF)
    if("cxx_std_20" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
        include(CheckCXXSourceCompiles)
        set(CMAKE_CXX_STANDARD 20)
        set(CMAKE_CXX_STANDARD_REQUIRED ON)
        set(CMAKE_CXX_EXTENSIONS OFF)
        # Match the concepts and templated lambdas used by the guide.
        check_cxx_source_compiles([[
#if !defined(__cpp_concepts) || __cpp_concepts < 201907L
#  error "C++20 concepts are required"
#endif
#if !defined(__cpp_generic_lambdas) || __cpp_generic_lambdas < 201707L
#  error "explicit template parameter lists for lambdas are required"
#endif
int main() { return 0; }
]] _omnirefl_cxx20_language_probe)
        set(_omnirefl_has_cxx20_language "${_omnirefl_cxx20_language_probe}")
    endif()
    set(${result} "${_omnirefl_has_cxx20_language}" PARENT_SCOPE)
endfunction()

function(omnirefl_check_cxx23_library result)
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
