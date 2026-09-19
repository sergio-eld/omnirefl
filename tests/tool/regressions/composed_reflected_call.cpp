#include <omnirefl/reflected_scope.hpp>

#include <gtest/gtest.h>

#include <functional>
#include <tuple>

namespace composed_reflected_call {

struct mapper {
  template <typename T>
  std::size_t operator()(omni::type_t<T>) const {
    T object{};
    return omni::reflected_call(
      [](auto record) -> std::size_t {
        return std::tuple_size<decltype(record.public_fields())>::value;
      },
      object);
  }
};

template <typename T>
std::size_t compose() {
  return std::invoke(mapper{}, omni::type_t<T>{});
}

struct record {
  int value;
};

TEST(composed_reflected_call, uses_the_outer_instantiation_for_completeness) {
  EXPECT_EQ(1U, compose<record>());
}

} // namespace composed_reflected_call
