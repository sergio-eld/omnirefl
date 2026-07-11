#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <tuple>

namespace regression_public_nested_in_private_parent {

struct public_parent {
  private:
  struct private_parent {
    public:
    struct public_child {
      int value;
    };
  };

  public:
  private_parent::public_child exposed;
};

} // namespace regression_public_nested_in_private_parent

// FIXME: A public leaf under a private enclosing record is rendered through
// its inaccessible qualified name instead of the valid public field route.
TEST(regression, DISABLED_public_nested_type_uses_its_public_field_path) {
  regression_public_nested_in_private_parent::public_parent input{};

  EXPECT_TRUE(omni::reflected_call(
    [](auto binding) -> bool {
      using fields_t = decltype(binding.public_fields());
      using child_t = typename std::tuple_element_t<0, fields_t>::type;
      return omni::is_reflected<child_t>::value;
    },
    input));
}
