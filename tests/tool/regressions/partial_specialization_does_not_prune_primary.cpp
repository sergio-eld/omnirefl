#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <string_view>
#include <tuple>

namespace regression_partial_specialization_does_not_prune_primary {

template <typename T>
struct dependency {
  int primary_value;
};

template <typename T>
struct dependency<T *> {
  int partial_value;
};

struct record {
  dependency<int> primary;
  dependency<int *> partial;
};

} // namespace regression_partial_specialization_does_not_prune_primary

TEST(regression, partial_specialization_does_not_prune_primary) {
  using namespace regression_partial_specialization_does_not_prune_primary;

  record input{};
  EXPECT_EQ(std::string_view{"primary_value"},
    omni::reflected_call(
      [](auto binding) -> std::string_view {
        using record_fields_t = decltype(binding.public_fields());
        using primary_t =
          typename std::tuple_element_t<0, record_fields_t>::type;
        using fields_t = decltype(omni::reflected<primary_t>().public_fields());

        static_assert(1 == std::tuple_size_v<fields_t>);
        return std::tuple_element_t<0, fields_t>::name();
      },
      input));
}
