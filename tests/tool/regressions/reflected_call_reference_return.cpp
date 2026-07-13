#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

namespace regression_reflected_call_reference_return {

struct record {
  int value;
};

} // namespace regression_reflected_call_reference_return

// FIXME(high): the tool-run binding interface cannot substitute a generic
// visitor whose explicit return type refers to the bound value. The real call
// should preserve a reference to an lvalue-bound field.
TEST(regression, DISABLED_reflected_call_preserves_reference_return) {
  regression_reflected_call_reference_return::record input{7};

  int &result = omni::reflected_call(
    [](auto binding) -> decltype((binding.value.value)) {
      return binding.value.value;
    },
    input);
  result = 19;

  EXPECT_EQ(19, input.value);
}
