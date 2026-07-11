#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <array>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

namespace regression_private_nested_type_through_wrappers {

struct public_parent {
  private:
  struct vector_child {
    int vector_value;
  };

  struct tuple_child {
    int tuple_value;
  };

  struct variant_child {
    int variant_value;
  };

  public:
  std::vector<vector_child> vector_values;
  std::tuple<tuple_child> tuple_values;
  std::variant<variant_child, int> variant_values;
};

} // namespace regression_private_nested_type_through_wrappers

TEST(regression, private_nested_type_through_wrappers) {
  using namespace regression_private_nested_type_through_wrappers;

  public_parent value{};
  const std::array expected{
    std::string_view{"vector_value"},
    std::string_view{"tuple_value"},
    std::string_view{"variant_value"},
  };

  EXPECT_EQ(expected,
    omni::reflected_call(
      [](auto binding) -> std::array<std::string_view, 3> {
        using fields_t = decltype(binding.public_fields());
        using vector_t = typename std::tuple_element_t<0, fields_t>::type;
        using tuple_t = typename std::tuple_element_t<1, fields_t>::type;
        using variant_t = typename std::tuple_element_t<2, fields_t>::type;

        using vector_child_t = typename vector_t::value_type;
        using tuple_child_t = std::tuple_element_t<0, tuple_t>;
        using variant_child_t = std::variant_alternative_t<0, variant_t>;

        using vector_fields_t =
          decltype(omni::reflected<vector_child_t>().public_fields());
        using tuple_fields_t =
          decltype(omni::reflected<tuple_child_t>().public_fields());
        using variant_fields_t =
          decltype(omni::reflected<variant_child_t>().public_fields());

        return {
          std::tuple_element_t<0, vector_fields_t>::name(),
          std::tuple_element_t<0, tuple_fields_t>::name(),
          std::tuple_element_t<0, variant_fields_t>::name(),
        };
      },
      value));
}
