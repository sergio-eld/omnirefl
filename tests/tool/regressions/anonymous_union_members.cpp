#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <array>
#include <string_view>

namespace regression_anonymous_union_members {

struct record {
  union {
    int integer;
    float real;
  };
  int tag;
};

} // namespace regression_anonymous_union_members

// FIXME(medium): the unnamed union container is emitted as an empty-named field
// (`owner.`), while its promoted public members are omitted from metadata.
TEST(regression, DISABLED_anonymous_union_exposes_promoted_public_members) {
  const std::array<std::string_view, 3> expected{"integer", "real", "tag"};

  EXPECT_EQ(expected,
    omni::reflected_call(
      [](auto meta) -> std::array<std::string_view, 3> {
        return omni::compat::apply(
          [](auto... field) -> std::array<std::string_view, 3> {
            return {{field.name()...}};
          },
          meta.public_fields());
      },
      omni::type<regression_anonymous_union_members::record>));
}
