
#include "odr_test.hpp"

#include <gtest/gtest.h>

// note: this will not be picked up by gtest...
TEST(odr_test, inside_test_static_odr_cpp) {
  static const odr_test::input k_input{815,
    "oceanic",
    {
      "you",
      "can't",
      "see",
      "me",
    }};

  static const std::vector<std::string> k_expected{
    "m_int: 815",
    "m_string: oceanic",
    "m_vec: [you, can't, see, me]",
  };

  EXPECT_EQ(k_expected,
    omni::reflected_call(odr_test::get_field_name_values, k_input));
  EXPECT_EQ(k_expected, odr_test::in_header_call(k_input));
}

std::vector<std::string> odr_test::get_field_name_values_from(
  const odr_test::input &i) {
  return odr_test::in_header_call(i);
}
