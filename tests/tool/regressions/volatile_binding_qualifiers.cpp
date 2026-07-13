#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <tuple>

namespace regression_volatile_binding_qualifiers {

struct record {
  int value;
};

} // namespace regression_volatile_binding_qualifiers

// FIXME(low): generated field accessors discard volatile from bound records.
TEST(regression, DISABLED_volatile_binding_preserves_field_qualifiers) {
  volatile regression_volatile_binding_qualifiers::record input{17};

  EXPECT_EQ(17,
    omni::reflected_call(
      [](auto binding) -> int {
        return std::get<0>(binding.public_fields()).value();
      },
      input));
}
