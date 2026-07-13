#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <string_view>

namespace regression_inline_namespace {

inline namespace v1 {
struct record {
  int value;
};
} // namespace v1

} // namespace regression_inline_namespace

// FIXME(high): generated forward declarations render an inline namespace as a
// normal namespace, making the source's later inline definition ill-formed.
TEST(regression, DISABLED_generated_declaration_preserves_inline_namespace) {
  EXPECT_EQ("regression_inline_namespace::v1::record",
    omni::reflected_call(
      [](auto meta) -> std::string_view {
        return meta.qualified_type_name();
      },
      omni::type<regression_inline_namespace::record>));
}
