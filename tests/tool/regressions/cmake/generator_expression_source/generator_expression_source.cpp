#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

struct record {
  int value;
};

// FIXME(medium): omni_reflected_target retains generator-expression source
// entries initially but then filters them out because the expression does not
// end with a recognized source suffix.
TEST(regression, DISABLED_generator_expression_source_is_instrumented) {
  EXPECT_EQ(1,
    omni::reflected_call(
      [](auto binding) -> int { return binding.value.value; }, record{1}));
}
