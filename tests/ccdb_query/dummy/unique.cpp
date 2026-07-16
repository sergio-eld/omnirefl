#if !defined UNIQUE_TARGET
#  error UNIQUE_TARGET must be defined
#endif
#if !defined CCDB_BACKSLASH
#  error CCDB_BACKSLASH must be defined
#endif

#include <string_view>

static_assert("left\\right" == std::string_view{CCDB_BACKSLASH});

int unique_value() {
  return UNIQUE_TARGET;
}
