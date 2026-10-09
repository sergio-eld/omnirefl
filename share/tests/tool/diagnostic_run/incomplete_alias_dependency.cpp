// Expected warning: the dependency route reaches `std::pair<incomplete, int>`,
// which cannot be instantiated without a definition for `incomplete`. The root
// must remain reflectable, and the tool must not crash inside Clang Sema.
#include <gtest/gtest.h>
#include <omnirefl/reflected_scope.hpp>

#include <tuple>
#include <utility>

namespace diagnostic_incomplete_alias_dependency {

struct incomplete;

struct record {
  using value_type = std::pair<incomplete, int>;

  int value;
};

} // namespace diagnostic_incomplete_alias_dependency

TEST(diagnostic, incomplete_alias_dependency) {
  diagnostic_incomplete_alias_dependency::record value{815};
  omni::reflected_call(
    [](auto binding) -> void {
      EXPECT_EQ(815, std::get<0>(binding.public_fields()).value());
    },
    value);
}
