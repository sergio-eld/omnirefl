#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <string_view>

namespace regression_public_nested_direct_input {

struct parent {
  struct child {
    int value;
  };
};

struct holder {
  parent::child nested;
};

} // namespace regression_public_nested_direct_input

TEST(regression, public_nested_direct_input) {
  using namespace regression_public_nested_direct_input;

  holder dependency_route{};
  omni::reflected_call([](auto) -> void {}, dependency_route);

  parent::child direct{};
  EXPECT_EQ(std::string_view{"parent::child"},
    omni::reflected_call(
      [](auto binding) -> std::string_view { return binding.type_name(); },
      direct));
}
