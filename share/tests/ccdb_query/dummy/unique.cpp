#if !defined UNIQUE_TARGET
#  error UNIQUE_TARGET must be defined
#endif
#if !defined CCDB_BACKSLASH
#  error CCDB_BACKSLASH must be defined
#endif

#include <cstddef>

// C++11 has no constexpr standard-library string comparison.
template <std::size_t N, std::size_t M>
constexpr bool same_text(const char (&a)[N],
  const char (&b)[M],
  std::size_t i = 0) {
  return N == M && (N == i || (a[i] == b[i] && same_text(a, b, i + 1)));
}

static_assert(same_text("left\\right", CCDB_BACKSLASH),
  "the compile definition must preserve the backslash");

int unique_value() {
  return UNIQUE_TARGET;
}
