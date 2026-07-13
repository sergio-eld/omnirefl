#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <array>

namespace regression_inherited_field_name_hiding {

struct left_base {
  int shared;
  int left;
};

struct right_base {
  int shared;
  int right;
};

struct derived : left_base, right_base {
  int shared;
  int own;
};

} // namespace regression_inherited_field_name_hiding

// FIXME(high): inherited field wrappers access a derived binding as
// `owner.field` instead of casting to the field's declaring base. Hidden base
// fields therefore silently read the derived field with the same name.
TEST(regression, DISABLED_inherited_fields_use_their_declaring_base) {
  using regression_inherited_field_name_hiding::derived;
  using regression_inherited_field_name_hiding::left_base;
  using regression_inherited_field_name_hiding::right_base;

  derived input{};
  input.left_base::shared = 1;
  input.left = 2;
  input.right_base::shared = 3;
  input.right = 4;
  input.shared = 5;
  input.own = 6;

  const std::array<int, 6> expected{1, 2, 3, 4, 5, 6};
  EXPECT_EQ(expected,
    omni::reflected_call(
      [](auto binding) -> std::array<int, 6> {
        return omni::compat::apply(
          [](auto... field) -> std::array<int, 6> {
            return {{field.value()...}};
          },
          binding.public_fields());
      },
      input));
}
