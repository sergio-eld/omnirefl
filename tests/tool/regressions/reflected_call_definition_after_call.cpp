#include <gtest/gtest.h>

#include <omnirefl/reflection.hpp>

#include <cstddef>
#include <tuple>

namespace regression_reflected_call_definition_after_call {

struct record;

std::size_t field_count(const record &value) {
  return omni::reflected_call(
    [](auto binding) -> std::size_t {
      return std::tuple_size<decltype(binding.public_fields())>::value;
    },
    value);
}

struct record {
  int value;
};

// fixme(high): reject the argument when its definition occurs after the
// reflected_call instead of accepting a later whole-translation-unit
// definition.
TEST(regression, reflected_call_definition_must_precede_call) {
  EXPECT_EQ(std::size_t{1}, field_count({}));
}

} // namespace regression_reflected_call_definition_after_call
