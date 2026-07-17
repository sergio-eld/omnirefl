#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <tuple>

namespace diagnostic_virtual_base_dependency {

struct root {
  int value;
};

struct left: virtual root {};
struct right: virtual root {};
// The virtual base is inherited through `left` and `right`.
struct virtual_dependency: left, right {};

template <typename T>
struct template_virtual_dependency: virtual root {
  T payload;
};

struct first_record {
  virtual_dependency dependency;
  int own_value;
};

struct second_record {
  virtual_dependency dependency;
  int own_value;
};

struct template_record {
  template_virtual_dependency<int> dependency;
  int own_value;
};

} // namespace diagnostic_virtual_base_dependency

TEST(diagnostic, virtual_base_dependency) {
  diagnostic_virtual_base_dependency::first_record first{};
  diagnostic_virtual_base_dependency::second_record second{};
  diagnostic_virtual_base_dependency::template_record template_value{};
  first.own_value = 23;
  second.own_value = 29;
  template_value.own_value = 31;

  omni::reflected_call(
    [](auto first_binding, auto second_binding, auto template_binding) -> void {
      static_assert(
        2 == std::tuple_size_v<decltype(first_binding.public_fields())>);
      static_assert(
        2 == std::tuple_size_v<decltype(second_binding.public_fields())>);
      static_assert(
        2 == std::tuple_size_v<decltype(template_binding.public_fields())>);

      EXPECT_EQ(23, std::get<1>(first_binding.public_fields()).value());
      EXPECT_EQ(29, std::get<1>(second_binding.public_fields()).value());
      EXPECT_EQ(31, std::get<1>(template_binding.public_fields()).value());
    },
    first,
    second,
    template_value);
}
