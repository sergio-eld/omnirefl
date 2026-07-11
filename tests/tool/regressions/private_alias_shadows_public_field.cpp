#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <tuple>

namespace regression_private_alias_shadows_public_field {

struct public_parent {
  private:
  struct private_child {
    int value;
  };

  using value_type = private_child;

  public:
  private_child exposed;
};

} // namespace regression_private_alias_shadows_public_field

// FIXME: The private alias route shadows the valid public field route, leaving
// the exposed private child without usable generated metadata.
TEST(regression, DISABLED_private_alias_does_not_shadow_public_field_path) {
  regression_private_alias_shadows_public_field::public_parent input{};

  EXPECT_TRUE(omni::reflected_call(
    [](auto binding) -> bool {
      using fields_t = decltype(binding.public_fields());
      using child_t = typename std::tuple_element_t<0, fields_t>::type;
      return omni::is_reflected<child_t>::value;
    },
    input));
}
