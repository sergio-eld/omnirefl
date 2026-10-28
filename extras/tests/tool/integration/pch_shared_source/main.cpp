#include "model.hpp"

#include <omnirefl/reflected_scope.hpp>

#include <cstddef>
#include <iostream>
#include <tuple>

struct field_count {
  template <typename _M>
  std::size_t operator()(omni::record_meta_t<_M> record) const {
    return std::tuple_size<decltype(record.accessible_data_members())>::value;
  }
};

int main() {
  const auto count = omni::reflected_call(field_count{}, omni::type_t<model>{});
  const std::size_t expected = EXTRA_FIELD ? 2 : 1;
  if (count != expected) {
    std::cerr << "Expected " << expected << " fields, got " << count << '\n';
    return 1;
  }
}
