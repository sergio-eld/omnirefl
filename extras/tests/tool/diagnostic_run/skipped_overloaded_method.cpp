// Expected warning: overloaded methods are skipped without failing generation.
#include <gtest/gtest.h>
#include <omnirefl/reflected_scope.hpp>

#include <tuple>

namespace diagnostic_skipped_overloaded_method {

struct record {
  int value;

  int read(int) const { return value; }
  int read(bool) const { return value; }
};

} // namespace diagnostic_skipped_overloaded_method

TEST(diagnostic, skipped_overloaded_method) {
  using diagnostic_skipped_overloaded_method::record;

  const int value = omni::reflected_call(
    [](auto binding) -> int {
      static_assert(std::tuple_size_v<decltype(binding.public_fields())> == 1,
        "ordinary fields must remain available");
      static_assert(std::tuple_size_v<decltype(binding.public_methods())> == 0,
        "overloaded methods must be skipped");

      return omni::compat::apply(
        [](auto field) { return field.value(); },
        binding.public_fields());
    },
    record{/*value=*/815});

  EXPECT_EQ(815, value);
}
