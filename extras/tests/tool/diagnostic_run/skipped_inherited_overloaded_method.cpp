// Expected warning: using-imported overloads make member pointers ambiguous.
#include <gtest/gtest.h>
#include <omnirefl/reflected_scope.hpp>

#include <tuple>

namespace diagnostic_skipped_inherited_overloaded_method {

struct base {
  int read(int) const { return 0; }
};

struct record: base {
  using base::read;

  int value;

  int read(bool) const { return value; }
};

} // namespace diagnostic_skipped_inherited_overloaded_method

TEST(diagnostic, skipped_inherited_overloaded_method) {
  using diagnostic_skipped_inherited_overloaded_method::record;

  const int value = omni::reflected_call(
    [](auto binding) -> int {
      static_assert(std::tuple_size_v<decltype(binding.public_fields())> == 1,
        "ordinary fields must remain available");
      static_assert(std::tuple_size_v<decltype(binding.public_methods())> == 0,
        "using-imported overloads must be skipped");

      return omni::compat::apply(
        [](auto field) { return field.value(); },
        binding.public_fields());
    },
    record{/*base=*/{}, /*value=*/815});

  EXPECT_EQ(815, value);
}
