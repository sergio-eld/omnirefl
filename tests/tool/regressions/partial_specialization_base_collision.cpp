#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <string_view>
#include <tuple>

namespace regression_partial_specialization_base_collision {

template <typename T>
struct base {
  int primary_value;
};

template <typename T>
struct base<T *> {
  int partial_value;
};

struct dependency_route {
  base<int> primary;
};

struct record: base<int *> {
  int own_value;
};

} // namespace regression_partial_specialization_base_collision

TEST(regression, partial_specialization_base_collision) {
  using namespace regression_partial_specialization_base_collision;

  dependency_route dependency{};
  omni::reflected_call([](auto) -> void {}, dependency);

  record input{};
  EXPECT_EQ(std::string_view{"own_value"},
    omni::reflected_call(
      [](auto binding) -> std::string_view {
        using fields_t = decltype(binding.public_fields());

        static_assert(1 == std::tuple_size_v<fields_t>);
        return std::tuple_element_t<0, fields_t>::name();
      },
      input));
}
