// Expected warnings: alias discovery reaches explicit and partial template
// specializations. Only primary template specializations are supported, so
// both routes are skipped without preventing reflection of the root.
#include <gtest/gtest.h>
#include <omnirefl/reflected_scope.hpp>

#include <tuple>

namespace diagnostic_lazy_specialization_dependency {

struct explicit_value;

template <typename T>
struct explicit_route {
  using value_type = T;
};

template <>
struct explicit_route<explicit_value>;

template <typename T>
struct partial_route {
  using value_type = T;
};

template <typename T>
struct partial_route<T *> {
  using value_type = T;
};

struct record {
  using first_type = explicit_route<explicit_value>;
  using second_type = partial_route<int *>;

  int value;
};

} // namespace diagnostic_lazy_specialization_dependency

TEST(diagnostic, lazy_specialization_dependency) {
  diagnostic_lazy_specialization_dependency::record value{815};
  omni::reflected_call(
    [](auto binding) -> void {
      EXPECT_EQ(815, std::get<0>(binding.public_fields()).value());
    },
    value);
}
