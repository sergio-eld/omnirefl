#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <type_traits>

#if defined(OMNI_REGRESSION_IMACROS)
#  if !defined(OMNI_IMACROS_ACTIVE)
#    error "-imacros input was not applied"
#  endif
#elif defined(OMNI_REGRESSION_PTHREAD)
#  if !defined(_REENTRANT)
#    error "-pthread frontend macro was not applied"
#  endif
#elif defined(OMNI_REGRESSION_OPENMP)
#  if !defined(_OPENMP)
#    error "-fopenmp frontend mode was not applied"
#  endif
#elif defined(OMNI_REGRESSION_NO_EXCEPTIONS)
#  if defined(__cpp_exceptions)
#    error "-fno-exceptions frontend mode was not applied"
#  endif
#elif defined(OMNI_REGRESSION_NO_RTTI)
#  if defined(__GXX_RTTI)
#    error "-fno-rtti frontend mode was not applied"
#  endif
#elif defined(OMNI_REGRESSION_SHORT_WCHAR)
static_assert(2 == sizeof(wchar_t), "-fshort-wchar was not applied");
#elif defined(OMNI_REGRESSION_UNSIGNED_CHAR)
static_assert(!std::is_signed<char>::value, "-funsigned-char was not applied");
#elif defined(OMNI_REGRESSION_PACK_STRUCT)
struct packed_probe {
  char first;
  int second;
};
static_assert(5 == sizeof(packed_probe), "-fpack-struct was not applied");
#elif defined(OMNI_REGRESSION_NO_OPERATOR_NAMES)
constexpr int and = 3;
static_assert(3 == and, "-fno-operator-names was not applied");
#elif defined(OMNI_REGRESSION_NO_CHAR8_T)
static_assert(std::is_same<decltype(u8'x'), char>::value,
  "-fno-char8_t was not applied");
#elif defined(OMNI_REGRESSION_AVX2)
#  if !defined(__AVX2__)
#    error "-mavx2 target features were not applied"
#  endif
#elif defined(OMNI_REGRESSION_NO_ACCESS_CONTROL)
class access_probe {
  using hidden = int;
};
static_assert(std::is_same<access_probe::hidden, int>::value,
  "-fno-access-control was not applied");
#elif defined(OMNI_REGRESSION_NO_SIZED_DEALLOCATION)
#  if defined(__cpp_sized_deallocation)
#    error "-fno-sized-deallocation was not applied"
#  endif
#else
#  error "driver option regression selector is required"
#endif

// FIXME(high): frontend-affecting compiler options selected above are dropped
// while omnirefl builds its AST, so the tool can parse a different program or
// fail code that the configured compiler accepts.
TEST(regression, DISABLED_driver_option_reaches_frontend_ast) {
  SUCCEED();
}
