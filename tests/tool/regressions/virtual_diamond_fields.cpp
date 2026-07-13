#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <tuple>

namespace regression_virtual_diamond_fields {

struct root {
  int value;
};

struct left : virtual root {};
struct right : virtual root {};
struct record : left, right {};

} // namespace regression_virtual_diamond_fields

// FIXME(medium): public field flattening traverses both paths through a virtual
// diamond and exposes the shared base field twice.
TEST(regression, DISABLED_virtual_base_fields_are_not_duplicated) {
  regression_virtual_diamond_fields::record input;
  input.value = 23;

  EXPECT_TRUE(omni::reflected_call(
    [](auto binding) -> bool {
      return 1 == std::tuple_size_v<decltype(binding.public_fields())>
        && 23 == std::get<0>(binding.public_fields()).value();
    },
    input));
}
