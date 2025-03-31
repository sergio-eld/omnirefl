
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

namespace example_impl {
struct print_field_names_simple_t {
  template <typename T>
  void operator()(const T &t, std::vector<std::string> &out) const {
    const auto fields = omni::reflected(t).fields;
    out.reserve(fields.size());
    for (const auto &f : fields)
      out.emplace_back(std::string(f.name));
  }
} const static print_field_names_simple{};
} // namespace example_impl

TEST(print_names, in_header_struct) {
  const example::in_header_person p{};
  const static std::vector<std::string> expected{
    "name",
    "age",
  };
  std::vector<std::string> result;
  omni::reflected_call(example_impl::print_field_names_simple, p, result);
  ASSERT_EQ(expected, result);
}

TEST(print_names, in_cpp_struct) {
  const example::in_cpp_person p{};
  const static std::vector<std::string> expected{
    "name",
    "age",
  };
  std::vector<std::string> result;
  omni::reflected_call(example_impl::print_field_names_simple, p, result);
  ASSERT_EQ(expected, result);
}

TEST(print_names, in_cpp_local_unnamed_struct) {
  struct {
    std::string name;
    int age;
  } p{};
  const static std::vector<std::string> expected{
    "name",
    "age",
  };
  std::vector<std::string> result;
  omni::reflected_call(example_impl::print_field_names_simple, p, result);
  ASSERT_EQ(expected, result);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
