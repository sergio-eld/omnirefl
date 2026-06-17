
#include <gtest/gtest.h>
#include "structs.h" //< todo: move to a separate file

#include <omnirefl/reflection.hpp>

namespace {

TEST(print_names, simple) {
  {
    const example_types::championship v{};
    const static std::vector<std::string> expected{
      "name",
      "title",
    };
    const std::vector<std::string> result =
      omni::reflected_call(example_impl::print_field_names_simple, v);
    EXPECT_EQ(expected, result);
  }

  {
    const example_types::wrestler v{};
    const static std::vector<std::string> expected{
      "name",
      "age",
      "catchphrase",
      "titles",
      "info",
    };
    const std::vector<std::string> result =
      omni::reflected_call(example_impl::print_field_names_simple, v);
    EXPECT_EQ(expected, result);
  }
}

} // namespace
