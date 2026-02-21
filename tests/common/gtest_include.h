#pragma once

#ifdef OMNI_TOOL_RUN
#  ifdef GTEST_HAS_FILE_SYSTEM
#    undef GTEST_HAS_FILE_SYSTEM
#  endif
#  define GTEST_HAS_FILE_SYSTEM 0

// gtest 1.12.x on Windows uses std::wstring_convert in gtest-port.h.
// MSVC STL removes it in C++26 mode; for AST-only tool runs, provide a stub.
#  if defined(_MSC_VER) \
    && ((defined(_MSVC_LANG) && _MSVC_LANG >= 202400L) \
      || (__cplusplus >= 202400L))

#    include <memory>
#    include <string>

namespace std {
template <class Codecvt,
  class Elem = wchar_t,
  class Wide_alloc = allocator<Elem>,
  class Byte_alloc = allocator<char>>
class wstring_convert {
  public:
  using wide_string = basic_string<Elem, char_traits<Elem>, Wide_alloc>;
  using byte_string = basic_string<char, char_traits<char>, Byte_alloc>;

  wstring_convert() = default;

  wide_string from_bytes(const byte_string &);
  wide_string from_bytes(const char *);
  wide_string from_bytes(const char *, const char *);
};
} // namespace std

#  endif
#endif

#include <gtest/gtest.h>
