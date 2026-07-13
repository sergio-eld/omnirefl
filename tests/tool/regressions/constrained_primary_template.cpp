#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <tuple>
#include <type_traits>

namespace regression_constrained_primary_template {

template <typename T>
  requires std::is_integral_v<T>
struct record {
  T value;
};

} // namespace regression_constrained_primary_template

// FIXME(high): generated primary-template forward declarations omit requires
// clauses and conflict with the source definition.
TEST(regression, DISABLED_constrained_primary_template_is_reflectable) {
  using record = regression_constrained_primary_template::record<int>;
  (void)sizeof(record);

  EXPECT_EQ(1U,
    omni::reflected_call(
      []<omni::meta Meta>(Meta meta) -> std::size_t {
        return std::tuple_size_v<decltype(meta.public_fields())>;
      },
      omni::type<record>));
}
