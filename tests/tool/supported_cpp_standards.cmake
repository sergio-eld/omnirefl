function(_omnirefl_supported_cpp_standards out)
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
