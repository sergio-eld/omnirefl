#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <array>

template <typename T>
struct base {
  T value;
};

struct record : base<int>, base<double> {};

// FIXME(high): inherited fields from distinct specializations of the same
// primary-template base are emitted with the same owner metadata. Binding the
// fields to the derived record then produces ambiguous member access.
TEST(regression, DISABLED_multiple_primary_template_bases_keep_field_owners) {
  record input;
  static_cast<base<int> &>(input).value = 3;
  static_cast<base<double> &>(input).value = 4.5;

  EXPECT_EQ((std::array<double, 2>{3.0, 4.5}),
    omni::reflected_call(
      [](auto binding) -> std::array<double, 2> {
        return omni::compat::apply(
          [](auto... field) -> std::array<double, 2> {
            return {field.value()...};
          },
          binding.public_fields());
      },
      input));
}
