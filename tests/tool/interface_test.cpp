#include "api_structs.hpp"
#include <gtest/gtest.h>
#include "odr_test.hpp"

#include <omnirefl/reflection.hpp>

#include <utility>

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

#if defined(__cpp_concepts)
TEST(cpp20_template_lambdas, meta_type_token) {
  using interface_test::record_type_t;

  EXPECT_EQ("record_type_t",
    omni::reflected_call(
      [](omni::meta auto type) -> std::string {
        return type.type_name();
      },
      omni::type<record_type_t>));
}

TEST(cpp20_template_lambdas, record_binding) {
  using interface_test::record_type_t;

  record_type_t value{815, "oceanic"};

  static const std::vector<std::string> k_expected{"815", "oceanic"};

  EXPECT_EQ(k_expected,
    omni::reflected_call(
      [](omni::binding auto value) -> std::vector<std::string> {
        const auto fields = value.public_fields();
        return omni::compat::apply(
          [](const omni::field_binding auto &...field) {
            return std::vector<std::string>{
              omni::compat::to_string(field.value())...,
            };
          },
          fields);
      },
      value));
}
#endif

TEST(type_names, namespaced_record_type_t) {
  using interface_test::nested::namespaced_record_t;

  value_categories_test<namespaced_record_t>("namespaced_record_t",
    interface_test::type_name);
}

TEST(type_names, namespaced_record_qualified_type_name_t) {
  using interface_test::nested::namespaced_record_t;

  value_categories_test<namespaced_record_t>(
    "interface_test::nested::namespaced_record_t",
    interface_test::qualified_type_name);
}

TEST(type_names, nested_record_type_t) {
  using nested_record_t = interface_test::parent_record_t::nested_record_t;

  value_categories_test<nested_record_t>("parent_record_t::nested_record_t",
    interface_test::type_name);
}

TEST(type_names, nested_record_qualified_type_name_t) {
  using nested_record_t = interface_test::parent_record_t::nested_record_t;

  value_categories_test<nested_record_t>(
    "interface_test::parent_record_t::nested_record_t",
    interface_test::qualified_type_name);
}

TEST(type_names, namespaced_nested_record_type_t) {
  using nested_record_t =
    interface_test::nested::parent_record_t::nested_record_t;

  value_categories_test<nested_record_t>("parent_record_t::nested_record_t",
    interface_test::type_name);
}

TEST(type_names, namespaced_nested_record_qualified_type_name_t) {
  using nested_record_t =
    interface_test::nested::parent_record_t::nested_record_t;

  value_categories_test<nested_record_t>(
    "interface_test::nested::parent_record_t::nested_record_t",
    interface_test::qualified_type_name);
}

TEST(type_names, direct_type_token_record_type_t) {
  using interface_test::record_type_t;

  EXPECT_EQ("record_type_t",
    omni::reflected_call(interface_test::type_name,
      omni::type_t<record_type_t>{}));
}

TEST(type_names, direct_type_token_record_qualified_type_name_t) {
  using interface_test::record_type_t;

  EXPECT_EQ("interface_test::record_type_t",
    omni::reflected_call(interface_test::qualified_type_name,
      omni::type_t<record_type_t>{}));
}

#if OMNI_CPLUSPLUS >= 201402L
TEST(type_names, direct_type_token_variable_record_type_t) {
  using interface_test::record_type_t;

  EXPECT_EQ("record_type_t",
    omni::reflected_call(interface_test::type_name,
      omni::type<record_type_t>));
}
#endif

TEST(type_names, enum_type_t) {
  using interface_test::enum_type_t;

  value_categories_test<enum_type_t>("enum_type_t", interface_test::type_name);
}

TEST(type_names, enum_qualified_type_name_t) {
  using interface_test::enum_type_t;

  value_categories_test<enum_type_t>("interface_test::enum_type_t",
    interface_test::qualified_type_name);
}

TEST(type_names, direct_type_token_enum_type_t) {
  using interface_test::enum_type_t;

  EXPECT_EQ("enum_type_t",
    omni::reflected_call(interface_test::type_name,
      omni::type_t<enum_type_t>{}));
}

TEST(type_names, direct_type_token_enum_qualified_type_name_t) {
  using interface_test::enum_type_t;

  EXPECT_EQ("interface_test::enum_type_t",
    omni::reflected_call(interface_test::qualified_type_name,
      omni::type_t<enum_type_t>{}));
}

TEST(type_names, namespaced_enum_type_t) {
  using interface_test::nested::namespaced_enum_t;

  value_categories_test<namespaced_enum_t>("namespaced_enum_t",
    interface_test::type_name);
}

TEST(type_names, namespaced_enum_qualified_type_name_t) {
  using interface_test::nested::namespaced_enum_t;

  value_categories_test<namespaced_enum_t>(
    "interface_test::nested::namespaced_enum_t",
    interface_test::qualified_type_name);
}

TEST(fields, record_type_t) {
  using interface_test::record_type_t;
  namespace f = interface_test::fields;

  static const std::vector<std::string> k_expected{"first", "second"};

  value_categories_test<record_type_t>(k_expected, f::record_type_fields);
}

TEST(fields, namespaced_field_type_names) {
  using interface_test::nested::namespaced_field_types_t;
  namespace f = interface_test::fields;

  static const std::vector<std::string> k_expected{
    "interface_test::nested::namespaced_record_t",
    "interface_test::nested::namespaced_enum_t",
  };

  value_categories_test<namespaced_field_types_t>(k_expected,
    f::record_type_field_type_names);
}

TEST(fields, fully_qualified_duplicate_leaf_field_type_names) {
  using interface_test::nested::duplicate_leaf_field_types_t;
  namespace f = interface_test::fields;

  static const std::vector<std::string> k_expected{
    "interface_test::nested::left::duplicate_name_t",
    "interface_test::nested::right::duplicate_name_t",
  };

  value_categories_test<duplicate_leaf_field_types_t>(k_expected,
    f::record_type_field_type_names);
}

TEST(fields, sized_integer_field_types) {
  using interface_test::nested::sized_integer_field_types_t;
  namespace f = interface_test::fields;

  // These are canonical type-name strings reported by the tool. The source
  // fields use `std::uint16_t` and friends, but those are typedef aliases, not
  // canonical type names.
  static const std::vector<std::string> k_expected{
    "unsigned short",
    "int",
#if defined _MSC_VER
    "unsigned long long *",
#else
    "unsigned long *",
#endif
    "short **",
  };

  sized_integer_field_types_t lvalue{};
  const sized_integer_field_types_t const_lvalue{};

  EXPECT_EQ(k_expected,
    omni::reflected_call(f::record_type_field_type_names, lvalue));
  EXPECT_EQ(k_expected,
    omni::reflected_call(f::record_type_field_type_names, const_lvalue));
  EXPECT_EQ(k_expected,
    omni::reflected_call(f::record_type_field_type_names,
      sized_integer_field_types_t{}));
}

TEST(enumerators, enum_type_t) {
  using interface_test::enum_type_t;
  namespace en = interface_test::enumerators;

  value_categories_test<enum_type_t>("zero,one",
    en::enum_type_enumerators);
}

TEST(record_type_t, field_value_read) {
  using interface_test::record_type_t;
  namespace fv = interface_test::field_value_read;

  static const std::vector<std::string> k_expected{"815", "oceanic"};
  static const record_type_t k_input{815, "oceanic"};

  value_categories_read_test(k_expected,
    k_input,
    fv::record_type_field_values);

  // owning binding
  EXPECT_EQ(k_expected,
    omni::reflected_call(fv::record_type_field_values_own,
      record_type_t{k_input}));
}

TEST(record_type_t, reflected_rvalue_binding_can_be_named) {
  using interface_test::record_type_t;
  namespace fv = interface_test::field_value_read;

  static const std::vector<std::string> k_expected{"815", "oceanic"};

  EXPECT_EQ(k_expected,
    omni::reflected_call(interface_test::inline_examples::rvalue_binding_can_be_named,
      record_type_t{}));

  // Direct rvalue field access is intentionally invalid:
  // omni::reflected(record_type_t{}).public_fields();
  // omni::reflected(std::move(value)).public_fields();
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

  {
    record_type_t value{};
    omni::reflected_call(fw::record_type_field_write_call_t{k_expected}, value);
    EXPECT_EQ_FIELDS(k_expected, value);
  }

  EXPECT_EQ_FIELDS(k_expected,
    omni::reflected_call(fw::record_type_field_write_own_call_t{k_expected},
      record_type_t{}));

#undef EXPECT_EQ_FIELDS
}
