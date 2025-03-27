
#include "structs.h"

#include <omnirefl/refl.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace example {
struct in_cpp_person {
  std::string name;
  int age;
};
} // namespace example

TEST(print_names, in_header_struct) {
  const example::in_header_person p{};
  const static std::vector<std::string> expected{
    "name",
    "age",
  };
  std::vector<std::string> result;
  omni::reflected_call(example_impl::print_field_names_simple, p, result);
  EXPECT_EQ(expected, result);
}

// fixme: implement
// TEST(print_names, in_cpp_struct) {
//   const example::in_cpp_person p{};
//   const static std::vector<std::string> expected{
//     "name",
//     "age",
//   };
//   std::vector<std::string> result;
//   omni::reflected_call(example_impl::print_field_names_simple, p, result);
//   EXPECT_EQ(expected, result);
// }

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
