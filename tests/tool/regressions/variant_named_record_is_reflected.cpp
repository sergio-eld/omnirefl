#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <string_view>
#include <tuple>

namespace regression_variant_named_record_is_reflected {

struct dependency {
  int data;
};

template <typename T>
struct variant {
  T value;
};

} // namespace regression_variant_named_record_is_reflected

TEST(regression, variant_named_record_is_reflected) {
  using namespace regression_variant_named_record_is_reflected;

  variant<dependency> value{};
  EXPECT_EQ(std::string_view{"value"},
    omni::reflected_call(
      [](auto binding) -> std::string_view {
        return std::get<0>(binding.public_fields()).name();
      },
      value));
}
