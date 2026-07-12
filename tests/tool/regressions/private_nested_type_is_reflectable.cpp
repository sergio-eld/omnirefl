#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <string_view>
#include <tuple>

namespace regression_private_nested_type_is_reflectable {

struct public_parent {
  private:
  struct private_child {
    int nested_value;
  };

  public:
  private_child value;
};

} // namespace regression_private_nested_type_is_reflectable

TEST(regression, private_nested_type_is_reflectable) {
  regression_private_nested_type_is_reflectable::public_parent input{};
  EXPECT_EQ(std::string_view{"nested_value"},
    omni::reflected_call(
      [](auto binding) -> std::string_view {
        using fields_t = decltype(binding.public_fields());
        using hidden_t = typename std::tuple_element_t<0, fields_t>::type;
        using hidden_fields_t =
          decltype(omni::reflected<hidden_t>().public_fields());

        static_assert(1 == std::tuple_size_v<hidden_fields_t>);
        return std::tuple_element_t<0, hidden_fields_t>::name();
      },
      input));
}
