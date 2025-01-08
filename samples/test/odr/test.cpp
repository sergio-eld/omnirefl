// todo: write a test

#include "get_field_names.hpp"

#include <gtest/gtest.h>

// testing several reflected .cpp files using custom reflected types and user implementation
// todo: ideally, `_call_impl` should not be exported as a symbol in the resulting cmake target
// binary
TEST(odr, compare_field_names) {
  static const std::vector<std::string> expected{"oceanic"};
  const odr::dummy d{};
  const auto fields_a = odr::get_field_names_a(d);
  const auto fields_b = odr::get_field_names_b(d);
  EXPECT_EQ(fields_a, expected);
  EXPECT_EQ(fields_a, fields_b);
  EXPECT_EQ(fields_a, fields_b);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
