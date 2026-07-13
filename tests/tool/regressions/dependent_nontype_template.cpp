#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <tuple>

namespace regression_dependent_nontype_template {

template <typename T, T Initial>
struct record {
  T value = Initial;
};

} // namespace regression_dependent_nontype_template

// FIXME(high): generated template declarations emit Clang's dependent type
// spelling instead of remapping it to the generated type parameter name.
TEST(regression, DISABLED_dependent_nontype_template_is_reflectable) {
  using record = regression_dependent_nontype_template::record<int, 31>;
  (void)sizeof(record);

  EXPECT_EQ(1U,
    omni::reflected_call(
      [](auto meta) -> std::size_t {
        return std::tuple_size_v<decltype(meta.public_fields())>;
      },
      omni::type<record>));
}
