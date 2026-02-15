#pragma once

#ifdef OMNI_TOOL_RUN
#  ifdef GTEST_HAS_FILE_SYSTEM
#    undef GTEST_HAS_FILE_SYSTEM
#  endif
// MSVC + CMake: CXX_STANDARD=23 is mapped to /std:c++latest.
// clang-cl/Clang driver may translate that to -std=c++26. On Windows,
// gtest/internal/gtest-port.h uses std::wstring_convert (removed in C++26),
// which breaks AST-only parsing. Disable gtest filesystem support for tool runs.
#  define GTEST_HAS_FILE_SYSTEM 0
#endif

#include <gtest/gtest.h>

