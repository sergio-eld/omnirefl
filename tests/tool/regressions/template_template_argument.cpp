#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <string_view>

namespace regression_template_template_argument {

template <typename T>
struct box {
  T value;
};

template <template <typename> typename Container>
struct record {
  Container<int> value;
};

} // namespace regression_template_template_argument

// FIXME(high): a concrete primary-template record with a template-template
// argument is incorrectly diagnosed as an incomplete reflected_call input.
TEST(regression, DISABLED_template_template_argument_is_reflectable) {
  using namespace regression_template_template_argument;

  EXPECT_EQ("record<box>",
    omni::reflected_call(
      [](auto meta) -> std::string_view { return meta.type_name(); },
      omni::type<record<box>>));
}
