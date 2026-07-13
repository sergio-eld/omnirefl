#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

struct record {
  int value;
};

// FIXME(high): the CMake wrapper disambiguates compile commands with a plain
// target-name substring. A target whose name is a suffix of another target's
// name therefore matches both commands and cannot be instrumented.
TEST(regression, DISABLED_ccdb_output_filter_matches_exact_target) {
  EXPECT_EQ(1,
    omni::reflected_call(
      [](auto binding) -> int { return binding.value.value; }, record{1}));
}
