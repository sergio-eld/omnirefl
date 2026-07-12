#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <tuple>

namespace regression_private_nested_type_through_public_base {

struct public_base {
  private:
  struct private_child {
    int value;
  };

  public:
  private_child exposed;
};

struct derived: public_base {
  int own_value;
};

} // namespace regression_private_nested_type_through_public_base

// FIXME: Public-base traversal drops the route needed to name a private child
// exposed through the base's public field.
TEST(regression,
  DISABLED_private_nested_type_is_reflected_through_public_base) {
  regression_private_nested_type_through_public_base::derived input{};

  EXPECT_TRUE(omni::reflected_call(
    [](auto binding) -> bool {
      using fields_t = decltype(binding.public_fields());
      using child_t = typename std::tuple_element_t<0, fields_t>::type;
      return omni::is_reflected<child_t>::value;
    },
    input));
}
