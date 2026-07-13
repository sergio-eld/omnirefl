#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

struct record {
  int value;
};

// FIXME(medium): omni_reflected_target filters source suffixes
// case-sensitively, rejecting the conventional uppercase .C C++ suffix that
// CMake itself recognizes.
TEST(regression, DISABLED_uppercase_cpp_source_is_instrumented) {
  EXPECT_EQ(1,
    omni::reflected_call(
      [](auto binding) -> int { return binding.value.value; }, record{1}));
}
