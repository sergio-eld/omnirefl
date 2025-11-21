#include "api_structs.hpp"

#include <gtest/gtest.h>
#include <omnirefl/reflected_call.hpp>

template <typename T, typename Visit>
void values_test(const std::string &expected, const Visit &visit) {
  T lvalue{};
  const T const_lvalue{};

  EXPECT_EQ(expected, omni::reflected_call(visit, lvalue));
  EXPECT_EQ(expected, omni::reflected_call(visit, const_lvalue));
  EXPECT_EQ(expected, omni::reflected_call(visit, T{}));
}

// todo: add test for namespaces

TEST(type_identity, tagged_type_t) {
  using interface_test::tagged_type_t;
  namespace ti = interface_test::type_identity;

  // omni::reflected_tagged_t<T>::name()
  values_test<tagged_type_t>("tagged_type_t",
    ti::tagged_type_name_reflected_tagged_t);

  // omni::reflected_tagged<T>().name()
  values_test<tagged_type_t>("tagged_type_t",
    ti::tagged_type_name_reflected_tagged_fn);

  // omni::reflected_tagged(t).name()
  values_test<tagged_type_t>("tagged_type_t",
    ti::tagged_type_name_reflected_tagged_lv);

  // omni::reflected_t<T>::name()
  values_test<tagged_type_t>("tagged_type_t", ti::tagged_type_name_reflected_t);

  // omni::reflected<T>().name()
  values_test<tagged_type_t>("tagged_type_t",
    ti::tagged_type_name_reflected_fn);

  // omni::reflected(t).name()
  values_test<tagged_type_t>("tagged_type_t",
    ti::tagged_type_name_reflected_lv);
}

TEST(type_identity, enum_type_t) {
  using interface_test::enum_type_t;
  namespace ti = interface_test::type_identity;

  // omni::reflected_enum_t<T>::name()
  values_test<enum_type_t>("enum_type_t", ti::enum_type_name_reflected_enum_t);

  // omni::reflected_enum<T>().name()
  values_test<enum_type_t>("enum_type_t", ti::enum_type_name_reflected_enum_fn);

  // omni::reflected_enum(e).name()
  values_test<enum_type_t>("enum_type_t", ti::enum_type_name_reflected_enum_lv);

  // omni::reflected_t<T>::name()
  values_test<enum_type_t>("enum_type_t", ti::enum_type_name_reflected_t);

  // omni::reflected<T>().name()
  values_test<enum_type_t>("enum_type_t", ti::enum_type_name_reflected_fn);

  // omni::reflected(e).name()
  values_test<enum_type_t>("enum_type_t", ti::enum_type_name_reflected_lv);
}
