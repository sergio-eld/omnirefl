
#include <gtest/gtest.h>
#include "structs.h" //< todo: move to a separate file

#include <omnirefl/reflection.hpp>

namespace ccdb_output_filter_test {

struct record {
  int value;
};

#if !defined CXX_STANDARD || CXX_STANDARD <= 11
struct read_value {
  template <typename Binding>
  int operator()(Binding binding) const {
    return binding.value.value;
  }
};
#endif

} // namespace ccdb_output_filter_test

namespace {

TEST(cmake_integration, compile_command_filter_matches_exact_target) {
#if defined CXX_STANDARD && 11 < CXX_STANDARD
  EXPECT_EQ(1,
    omni::reflected_call(
      [](auto binding) -> int { return binding.value.value; },
      ccdb_output_filter_test::record{1}));
#else
  EXPECT_EQ(1,
    omni::reflected_call(ccdb_output_filter_test::read_value{},
      ccdb_output_filter_test::record{1}));
#endif
}

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
