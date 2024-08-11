# Treat all warnings as errors.
# Options: 
#   ALL_WARNINGS - Maximum level for warnings (ON by default)
#   CLEAN_BUILD - Treat all warnings as errors (ON by default)
# Options are not applied if a project included this script is not a Top-Level project

get_directory_property(HAS_PARENT PARENT_DIRECTORY)

if (NOT HAS_PARENT)
    option(ALL_WARNINGS "Maximum level for warnings" ON)
    option(CLEAN_BUILD "Treat all warnings as errors" ON)

    if (ALL_WARNINGS)
        message(STATUS "${PROJECT_NAME}: Maximum warning level")

        if (MSVC)
            add_compile_options(/W4)
        else()
            add_compile_options(-Wall -Wextra -pedantic)
        endif()
    endif()

    if (CLEAN_BUILD)
        message(STATUS "${PROJECT_NAME}: Treating all warnings as errors")

        if (MSVC)
            add_compile_options(/WX)
        else()
            add_compile_options(-Werror)
        endif()
    endif()
endif ()