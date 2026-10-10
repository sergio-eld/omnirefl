#include <type_traits>

#if defined(OMNI_DRIVER_IMACROS)
#  if !defined(OMNI_IMACROS_ACTIVE)
#    error "-imacros input was not applied"
#  endif
#elif defined(OMNI_DRIVER_PTHREAD)
#  if !defined(_REENTRANT)
#    error "-pthread frontend macro was not applied"
#  endif
#elif defined(OMNI_DRIVER_OPENMP)
#  if !defined(_OPENMP)
#    error "-fopenmp frontend mode was not applied"
#  endif
#elif defined(OMNI_DRIVER_NO_EXCEPTIONS)
#  if defined(__cpp_exceptions)
#    error "-fno-exceptions frontend mode was not applied"
#  endif
#elif defined(OMNI_DRIVER_NO_RTTI)
#  if defined(__GXX_RTTI)
#    error "-fno-rtti frontend mode was not applied"
#  endif
#elif defined(OMNI_DRIVER_SHORT_WCHAR)
static_assert(2 == sizeof(wchar_t), "-fshort-wchar was not applied");
#elif defined(OMNI_DRIVER_UNSIGNED_CHAR)
static_assert(!std::is_signed<char>::value, "-funsigned-char was not applied");
#elif defined(OMNI_DRIVER_PACK_STRUCT)
struct packed_probe {
  char first;
  int second;
};
static_assert(5 == sizeof(packed_probe), "-fpack-struct was not applied");
#elif defined(OMNI_DRIVER_NO_OPERATOR_NAMES)
constexpr int and = 3;
static_assert(3 == and, "-fno-operator-names was not applied");
#elif defined(OMNI_DRIVER_NO_CHAR8_T)
static_assert(std::is_same<decltype(u8'x'), char>::value,
  "-fno-char8_t was not applied");
#elif defined(OMNI_DRIVER_AVX2)
#  if !defined(__AVX2__)
#    error "-mavx2 target features were not applied"
#  endif
#elif defined(OMNI_DRIVER_NO_ACCESS_CONTROL)
class access_probe {
  using hidden = int;
};
static_assert(std::is_same<access_probe::hidden, int>::value,
  "-fno-access-control was not applied");
#elif defined(OMNI_DRIVER_NO_SIZED_DEALLOCATION)
#  if defined(__cpp_sized_deallocation)
#    error "-fno-sized-deallocation was not applied"
#  endif
#else
#  error "driver option selector is required"
#endif
