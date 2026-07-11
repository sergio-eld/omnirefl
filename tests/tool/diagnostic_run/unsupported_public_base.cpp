#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <string_view>
#include <tuple>

namespace diagnostic_unsupported_public_base {

template <typename T>
struct base {
  T value;
};

template <typename T>
struct base<T *> {
  T *value;
};

struct record: base<int *> {
  int own_value;
};

} // namespace diagnostic_unsupported_public_base

TEST(diagnostic, unsupported_public_base) {
  diagnostic_unsupported_public_base::record input{};
  omni::reflected_call(
    [](auto binding) -> void {
      using fields_t = decltype(binding.public_fields());

      static_assert(1 == std::tuple_size_v<fields_t>);
      EXPECT_EQ(std::string_view{"own_value"},
        (std::tuple_element_t<0, fields_t>::name()));
    },
    input);
}
