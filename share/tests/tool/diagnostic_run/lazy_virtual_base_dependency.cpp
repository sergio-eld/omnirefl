// Expected warning: instantiating the alias dependency reveals a virtual base.
// The dependency is skipped while the root remains reflectable.
#include <gtest/gtest.h>
#include <omnirefl/reflected_scope.hpp>

#include <tuple>

namespace diagnostic_lazy_virtual_base_dependency {

struct base {};

template <typename T>
struct route: virtual base {
  using value_type = T;
};

struct record {
  using value_type = route<int>;

  int value;
};

} // namespace diagnostic_lazy_virtual_base_dependency

TEST(diagnostic, lazy_virtual_base_dependency) {
  diagnostic_lazy_virtual_base_dependency::record value{815};
  omni::reflected_call(
    [](auto binding) -> void {
      EXPECT_EQ(815, std::get<0>(binding.public_fields()).value());
    },
    value);
}
