// One source has target-specific definitions and standards, or no reflection.
#include <cstddef>
#include <iostream>

#ifdef REFLECTED_SOURCE
#include <omnirefl/reflected_scope.hpp>
#include <tuple>

namespace shared_source {

struct record {
  int first;
#ifdef EXTRA_FIELD
  int second;
#endif
  // Draft C++17 modes may report a value below 201703L.
#if (defined(_MSVC_LANG) && 201402L < _MSVC_LANG) || 201402L < __cplusplus
  int third;
#endif
};

struct field_count {
  template <typename _M>
  std::size_t operator()(omni::record_meta_t<_M> m) const {
    return std::tuple_size<decltype(m.accessible_data_members())>::value;
  }
};

std::size_t count_fields() {
  return omni::reflected_call(field_count{}, omni::type_t<record>{});
}

} // namespace shared_source
#else
int main() {
  std::cout << "Unreflected source\n";
  return 0;
}
#endif
