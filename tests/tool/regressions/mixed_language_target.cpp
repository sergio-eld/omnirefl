#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

extern "C" int regression_mixed_language_value(void);

namespace regression_mixed_language_target {

struct record {
  int value;
};

} // namespace regression_mixed_language_target

// FIXME(high): omni_reflected_target instruments .c sources and force-includes
// generated C++ metadata into their C compilation. Mixed C/C++ targets should
// instrument only their C++ translation units.
TEST(regression, DISABLED_mixed_language_target_instruments_only_cpp) {
  using regression_mixed_language_target::record;

  record input{regression_mixed_language_value()};
  EXPECT_EQ(7,
    omni::reflected_call(
      [](auto binding) -> int { return binding.value.value; }, input));
}
