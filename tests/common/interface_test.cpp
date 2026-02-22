#include "api_structs.hpp"
#include "gtest_include.h"
#include "odr_test.hpp"

#include <omnirefl/reflected_call.hpp>

TEST(odr_test, inside_interface_test_cpp) {
  static const odr_test::input k_input{815,
    "oceanic",
    {
      "you",
      "can't",
      "see",
      "me",
    }};

  static const std::vector<std::string> k_expected{
    "m_int: 815",
    "m_string: oceanic",
    "m_vec: [you, can't, see, me]",
  };

  EXPECT_EQ(k_expected,
    omni::reflected_call(odr_test::get_field_name_values, k_input));
  EXPECT_EQ(k_expected, odr_test::in_header_call(k_input));
}

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
  interface_test::record_type_t init,
  const Visit &visit) {
  interface_test::record_type_t lvalue = init;
  const interface_test::record_type_t const_lvalue = init;

  EXPECT_EQ(expected, omni::reflected_call(visit, lvalue));
  EXPECT_EQ(expected, omni::reflected_call(visit, const_lvalue));

  // prvalue tested outside with 'owning' reflected visitors
}

// todo: add test for namespaces

TEST(type_identity, record_type_t) {
  using interface_test::record_type_t;
  namespace ti = interface_test::type_identity;

  // omni::reflected_record_t<T>::name()
  value_categories_test<record_type_t>("record_type_t",
    ti::record_type_name_reflected_record_t);

  // omni::reflected_record<T>().name()
  value_categories_test<record_type_t>("record_type_t",
    ti::record_type_name_reflected_record_fn);

  // omni::reflected_record(t).name()
  value_categories_test<record_type_t>("record_type_t",
    ti::record_type_name_reflected_record_lv);

  // omni::reflected_t<T>::name()
  value_categories_test<record_type_t>("record_type_t",
    ti::record_type_name_reflected_t);

  // omni::reflected<T>().name()
  value_categories_test<record_type_t>("record_type_t",
    ti::record_type_name_reflected_fn);

  // omni::reflected(t).name()
  value_categories_test<record_type_t>("record_type_t",
    ti::record_type_name_reflected_lv);
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

TEST(fields, record_type_t) {
  using interface_test::record_type_t;
  namespace f = interface_test::fields;

  static const std::vector<std::string> k_expected{"first", "second"};

  // omni::reflected_record_t<T>::public_fields()
  value_categories_test<record_type_t>(k_expected,
    f::record_type_fields_reflected_record_t);

  // omni::reflected_record<T>().public_fields()
  value_categories_test<record_type_t>(k_expected,
    f::record_type_fields_reflected_record_fn);

  // omni::reflected_t<T>::public_fields()
  value_categories_test<record_type_t>(k_expected,
    f::record_type_fields_reflected_t);

  // omni::reflected<T>().public_fields()
  value_categories_test<record_type_t>(k_expected,
    f::record_type_fields_reflected_fn2);
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

TEST(record_type_t, field_value_read) {
  using interface_test::record_type_t;
  namespace fv = interface_test::field_value_read;

  static const std::vector<std::string> k_expected{"815", "oceanic"};
  static const record_type_t k_input{815, "oceanic"};

  // via type: reflected_record_t<T>::public_fields(t)
  value_categories_read_test(k_expected,
    k_input,
    fv::record_type_field_values_reflected_record_t);

  // via type: reflected_record<T>().public_fields(t)
  value_categories_read_test(k_expected,
    k_input,
    fv::record_type_field_values_reflected_record_fn);

  // via binding (non-owning): reflected_record(t).public_fields()
  value_categories_read_test(k_expected,
    k_input,
    fv::record_type_field_values_reflected_record_lv);

  // via type: reflected_t<T>::public_fields(t)
  value_categories_read_test(k_expected,
    k_input,
    fv::record_type_field_values_reflected_t);

  // via type: reflected<T>().public_fields(t)
  value_categories_read_test(k_expected,
    k_input,
    fv::record_type_field_values_reflected_fn2);

  // via binding (non-owning): reflected(t).public_fields()
  value_categories_read_test(k_expected,
    k_input,
    fv::record_type_field_values_reflected_lv2);

  // owning / prvalue cases

  // record: omni::reflected_record(T{...}).public_fields()
  EXPECT_EQ(k_expected,
    omni::reflected_call(fv::record_type_field_values_reflected_record_own,
      record_type_t{k_input}));

  // polymorphic: omni::reflected(T{...}).public_fields()
  EXPECT_EQ(k_expected,
    omni::reflected_call(fv::record_type_field_values_reflected_own,
      record_type_t{k_input}));
}

TEST(record_type_t, field_value_write) {
  using interface_test::record_type_t;
  namespace fw = interface_test::field_value_write;

#define EXPECT_EQ_FIELDS(lhs, rhs) \
  do { \
    EXPECT_EQ((lhs).first, (rhs).first); \
    EXPECT_EQ((lhs).second, (rhs).second); \
  } while (false)

  // note: mutation tests do not cover const or reference-to-const fields.
  static const record_type_t k_expected{47, "You can't see me"};

  // via type: reflected_record_t<T>::public_fields(t)  (non-owning)
  {
    record_type_t value{};
    omni::reflected_call(fw::record_type_field_write_reflected_record_t,
      value,
      k_expected);
    EXPECT_EQ_FIELDS(k_expected, value);
  }

  // via type: reflected_record<T>().public_fields(t)  (non-owning)
  {
    record_type_t value{};
    omni::reflected_call(fw::record_type_field_write_reflected_record_fn,
      value,
      k_expected);
    EXPECT_EQ_FIELDS(k_expected, value);
  }

  // via binding: reflected_record(t).public_fields()  (non-owning)
  {
    record_type_t value{};
    omni::reflected_call(fw::record_type_field_write_reflected_record_lv,
      value,
      k_expected);
    EXPECT_EQ_FIELDS(k_expected, value);
  }

  // via type: reflected_t<T>::public_fields(t)  (non-owning)
  {
    record_type_t value{};
    omni::reflected_call(fw::record_type_field_write_reflected_t,
      value,
      k_expected);
    EXPECT_EQ_FIELDS(k_expected, value);
  }

  // via type: reflected<T>().public_fields(t)  (non-owning)
  {
    record_type_t value{};
    omni::reflected_call(fw::record_type_field_write_reflected_fn2,
      value,
      k_expected);
    EXPECT_EQ_FIELDS(k_expected, value);
  }

  // via binding: reflected(t).public_fields()  (non-owning)
  {
    record_type_t value{};
    omni::reflected_call(fw::record_type_field_write_reflected_lv2,
      value,
      k_expected);
    EXPECT_EQ_FIELDS(k_expected, value);
  }

  // owning / prvalue cases

  // record: omni::reflected_record(T{...}).public_fields()
  EXPECT_EQ_FIELDS(k_expected,
    omni::reflected_call(fw::record_type_field_write_reflected_record_own,
      record_type_t{},
      k_expected));

  // polymorphic: omni::reflected(T{...}).public_fields()
  EXPECT_EQ_FIELDS(k_expected,
    omni::reflected_call(fw::record_type_field_write_reflected_own,
      record_type_t{},
      k_expected));

#undef EXPECT_EQ_FIELDS
}
