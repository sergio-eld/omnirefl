#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <tuple>

namespace regression_deprecated_public_field {

struct record {
  int current;
  [[deprecated("use current")]] int legacy;
};

} // namespace regression_deprecated_public_field

// FIXME(high): generated field accessors name deprecated public members and
// emit deprecation diagnostics from the generated header. Consumers that treat
// warnings as errors cannot reflect otherwise valid records.
TEST(regression,
  DISABLED_generated_accessors_do_not_warn_for_deprecated_field) {
  regression_deprecated_public_field::record input{4, 5};

  EXPECT_EQ(9,
    omni::reflected_call(
      [](auto binding) -> int {
        return omni::compat::apply(
          [](auto... field) -> int {
            return (field.value() + ...);
          },
          binding.public_fields());
      },
      input));
}
