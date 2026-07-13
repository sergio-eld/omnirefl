#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <tuple>

namespace regression_variadic_primary_template {

template <typename... T>
struct record {
  std::tuple<T...> value;
};

using reflected_record = record<int, double>;
static_assert(0 < sizeof(reflected_record), "instantiate specialization");

} // namespace regression_variadic_primary_template

// FIXME(high): generated metadata renders the primary-template pack as
// record<_T0> instead of record<_T0...>, producing an ill-formed partial
// specialization after the tool run succeeds.
TEST(regression, DISABLED_variadic_primary_template_expands_its_type_pack) {
  EXPECT_EQ(1,
    omni::reflected_call(
      [](auto meta) -> std::size_t {
        return std::tuple_size<decltype(meta.public_fields())>::value;
      },
      omni::type<regression_variadic_primary_template::reflected_record>));
}
