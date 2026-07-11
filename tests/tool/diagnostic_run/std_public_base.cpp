#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <memory>
#include <string_view>
#include <tuple>

namespace diagnostic_std_public_base {

struct record: std::enable_shared_from_this<record> {
  int value;

  explicit record(int value): value(value) {}
};

} // namespace diagnostic_std_public_base

TEST(diagnostic, std_public_base) {
  diagnostic_std_public_base::record input{1};
  EXPECT_EQ(std::string_view{"value"},
    omni::reflected_call(
      [](auto binding) -> std::string_view {
        using fields_t = decltype(binding.public_fields());

        static_assert(1 == std::tuple_size_v<fields_t>);
        return std::tuple_element_t<0, fields_t>::name();
      },
      input));
}
