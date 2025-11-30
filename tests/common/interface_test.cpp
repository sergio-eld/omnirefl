#include "api_structs.hpp"

#include <gtest/gtest.h>
#include <omnirefl/reflected_call.hpp>

template <typename T, typename Visit>
void value_categories_test(const std::string &expected, const Visit &visit) {
  T lvalue{};
  const T const_lvalue{};

  EXPECT_EQ(expected, omni::reflected_call(visit, lvalue));
  EXPECT_EQ(expected, omni::reflected_call(visit, const_lvalue));
  EXPECT_EQ(expected, omni::reflected_call(visit, T{}));
}

template <typename T, typename Visit>
void value_categories_test(const std::vector<std::string> &expected,
  const Visit &visit) {
  T lvalue{};
  const T const_lvalue{};

  EXPECT_EQ(expected, omni::reflected_call(visit, lvalue));
  EXPECT_EQ(expected, omni::reflected_call(visit, const_lvalue));
  EXPECT_EQ(expected, omni::reflected_call(visit, T{}));
}

template <typename Visit>
void value_categories_read_test(const std::vector<std::string> &expected,
  interface_test::tagged_type_t init,
  const Visit &visit) {
  interface_test::tagged_type_t lvalue = init;
  const interface_test::tagged_type_t const_lvalue = init;

  EXPECT_EQ(expected, omni::reflected_call(visit, lvalue));
  EXPECT_EQ(expected, omni::reflected_call(visit, const_lvalue));

  // prvalue tested outside with 'owning' reflected visitors
}

// todo: add test for namespaces

TEST(type_identity, tagged_type_t) {
  using interface_test::tagged_type_t;
  namespace ti = interface_test::type_identity;

  // omni::reflected_tagged_t<T>::name()
  value_categories_test<tagged_type_t>("tagged_type_t",
    ti::tagged_type_name_reflected_tagged_t);

  // omni::reflected_tagged<T>().name()
  value_categories_test<tagged_type_t>("tagged_type_t",
    ti::tagged_type_name_reflected_tagged_fn);

  // omni::reflected_tagged(t).name()
  value_categories_test<tagged_type_t>("tagged_type_t",
    ti::tagged_type_name_reflected_tagged_lv);

  // omni::reflected_t<T>::name()
  value_categories_test<tagged_type_t>("tagged_type_t",
    ti::tagged_type_name_reflected_t);

  // omni::reflected<T>().name()
  value_categories_test<tagged_type_t>("tagged_type_t",
    ti::tagged_type_name_reflected_fn);

  // omni::reflected(t).name()
  value_categories_test<tagged_type_t>("tagged_type_t",
    ti::tagged_type_name_reflected_lv);
}

TEST(type_identity, enum_type_t) {
  using interface_test::enum_type_t;
  namespace ti = interface_test::type_identity;

  // omni::reflected_enum_t<T>::name()
  value_categories_test<enum_type_t>("enum_type_t",
    ti::enum_type_name_reflected_enum_t);

  // omni::reflected_enum<T>().name()
  value_categories_test<enum_type_t>("enum_type_t",
    ti::enum_type_name_reflected_enum_fn);

  // omni::reflected_enum(e).name()
  value_categories_test<enum_type_t>("enum_type_t",
    ti::enum_type_name_reflected_enum_lv);

  // omni::reflected_t<T>::name()
  value_categories_test<enum_type_t>("enum_type_t",
    ti::enum_type_name_reflected_t);

  // omni::reflected<T>().name()
  value_categories_test<enum_type_t>("enum_type_t",
    ti::enum_type_name_reflected_fn);

  // omni::reflected(e).name()
  value_categories_test<enum_type_t>("enum_type_t",
    ti::enum_type_name_reflected_lv);
}

TEST(fields, tagged_type_t) {
  using interface_test::tagged_type_t;
  namespace f = interface_test::fields;

  static const std::vector<std::string> k_expected{"first", "second"};

  // omni::reflected_tagged_t<T>::fields()
  value_categories_test<tagged_type_t>(k_expected,
    f::tagged_type_fields_reflected_tagged_t);

  // omni::reflected_tagged<T>().fields()
  value_categories_test<tagged_type_t>(k_expected,
    f::tagged_type_fields_reflected_tagged_fn);

  // omni::reflected_t<T>::fields()
  value_categories_test<tagged_type_t>(k_expected,
    f::tagged_type_fields_reflected_t);

  // omni::reflected<T>().fields()
  value_categories_test<tagged_type_t>(k_expected,
    f::tagged_type_fields_reflected_fn2);
}

TEST(enumerators, enum_type_t) {
  using interface_test::enum_type_t;
  namespace en = interface_test::enumerators;

  // omni::reflected_enum_t<T>::enumerators()
  value_categories_test<enum_type_t>("zero,one",
    en::enum_type_enumerators_reflected_enum_t);

  // omni::reflected_enum<T>().enumerators()
  value_categories_test<enum_type_t>("zero,one",
    en::enum_type_enumerators_reflected_enum_fn);

  // omni::reflected_enum(e).enumerators()
  value_categories_test<enum_type_t>("zero,one",
    en::enum_type_enumerators_reflected_enum_lv);

  // omni::reflected_t<T>::enumerators()
  value_categories_test<enum_type_t>("zero,one",
    en::enum_type_enumerators_reflected_t);

  // omni::reflected<T>().enumerators()
  value_categories_test<enum_type_t>("zero,one",
    en::enum_type_enumerators_reflected_fn);

  // omni::reflected(e).enumerators()
  value_categories_test<enum_type_t>("zero,one",
    en::enum_type_enumerators_reflected_lv);
}

TEST(tagged_type_t, field_value_read) {
  using interface_test::tagged_type_t;
  namespace fv = interface_test::field_value_read;

  static const std::vector<std::string> k_expected{"815", "oceanic"};
  static const tagged_type_t k_input{815, "oceanic"};

  // via type: reflected_tagged_t<T>::fields(t)
  value_categories_read_test(k_expected,
    k_input,
    fv::tagged_type_field_values_reflected_tagged_t);

  // via type: reflected_tagged<T>().fields(t)
  value_categories_read_test(k_expected,
    k_input,
    fv::tagged_type_field_values_reflected_tagged_fn);

  // via binding (non-owning): reflected_tagged(t).fields()
  value_categories_read_test(k_expected,
    k_input,
    fv::tagged_type_field_values_reflected_tagged_lv);

  // via type: reflected_t<T>::fields(t)
  value_categories_read_test(k_expected,
    k_input,
    fv::tagged_type_field_values_reflected_t);

  // via type: reflected<T>().fields(t)
  value_categories_read_test(k_expected,
    k_input,
    fv::tagged_type_field_values_reflected_fn2);

  // via binding (non-owning): reflected(t).fields()
  value_categories_read_test(k_expected,
    k_input,
    fv::tagged_type_field_values_reflected_lv2);

  // owning / prvalue cases

  // tagged: omni::reflected_tagged(T{...}).fields()
  EXPECT_EQ(k_expected,
    omni::reflected_call(fv::tagged_type_field_values_reflected_tagged_own,
      tagged_type_t{k_input}));

  // polymorphic: omni::reflected(T{...}).fields()
  EXPECT_EQ(k_expected,
    omni::reflected_call(fv::tagged_type_field_values_reflected_own,
      tagged_type_t{k_input}));
}

TEST(tagged_type_t, field_value_write) {
  using interface_test::tagged_type_t;
  namespace fw = interface_test::field_value_write;

#define EXPECT_EQ_FIELDS(lhs, rhs) \
  do { \
    EXPECT_EQ((lhs).first, (rhs).first); \
    EXPECT_EQ((lhs).second, (rhs).second); \
  } while (false)

  // note: mutation tests do not cover const or reference-to-const fields.
  static const tagged_type_t k_expected{47, "You can't see me"};

  // via type: reflected_tagged_t<T>::fields(t)  (non-owning)
  {
    tagged_type_t value{};
    omni::reflected_call(fw::tagged_type_field_write_reflected_tagged_t,
      value,
      k_expected);
    EXPECT_EQ_FIELDS(k_expected, value);
  }

  // via type: reflected_tagged<T>().fields(t)  (non-owning)
  {
    tagged_type_t value{};
    omni::reflected_call(fw::tagged_type_field_write_reflected_tagged_fn,
      value,
      k_expected);
    EXPECT_EQ_FIELDS(k_expected, value);
  }

  // via binding: reflected_tagged(t).fields()  (non-owning)
  {
    tagged_type_t value{};
    omni::reflected_call(fw::tagged_type_field_write_reflected_tagged_lv,
      value,
      k_expected);
    EXPECT_EQ_FIELDS(k_expected, value);
  }

  // via type: reflected_t<T>::fields(t)  (non-owning)
  {
    tagged_type_t value{};
    omni::reflected_call(fw::tagged_type_field_write_reflected_t,
      value,
      k_expected);
    EXPECT_EQ_FIELDS(k_expected, value);
  }

  // via type: reflected<T>().fields(t)  (non-owning)
  {
    tagged_type_t value{};
    omni::reflected_call(fw::tagged_type_field_write_reflected_fn2,
      value,
      k_expected);
    EXPECT_EQ_FIELDS(k_expected, value);
  }

  // via binding: reflected(t).fields()  (non-owning)
  {
    tagged_type_t value{};
    omni::reflected_call(fw::tagged_type_field_write_reflected_lv2,
      value,
      k_expected);
    EXPECT_EQ_FIELDS(k_expected, value);
  }

  // owning / prvalue cases

  // tagged: omni::reflected_tagged(T{...}).fields()
  EXPECT_EQ_FIELDS(k_expected,
    omni::reflected_call(fw::tagged_type_field_write_reflected_tagged_own,
      tagged_type_t{},
      k_expected));

  // polymorphic: omni::reflected(T{...}).fields()
  EXPECT_EQ_FIELDS(k_expected,
    omni::reflected_call(fw::tagged_type_field_write_reflected_own,
      tagged_type_t{},
      k_expected));

#undef EXPECT_EQ_FIELDS
}
