#include "api_structs.hpp"
#include <gtest/gtest.h>
#include "odr_test.hpp"

#include <omnirefl/reflection.hpp>

#include <string>
#include <utility> // IWYU pragma: keep
#include <vector>

namespace interface_test {
namespace field_visibility {

struct left_base {
  int shared;
  int left;
};

struct right_base {
  int shared;
  int right;
};

struct derived_hides_bases: left_base, right_base {
  int shared;
  int own;
};

struct root_base {
  int shared;
  int root;
};

struct middle_base: root_base {
  int shared;
  int middle;
};

struct base_hides_base: middle_base {
  int own;
};

struct collect_name_values {
  template <typename... Field>
  std::vector<std::pair<std::string, int>> operator()(Field... field) const {
    return {{field.name(), field.value()}...};
  }
};

struct name_values {
  template <typename T>
  std::vector<std::pair<std::string, int>> operator()(
    omni::binding_t<T> binding) const {
    return omni::compat::apply(collect_name_values{}, binding.public_fields());
  }
};

} // namespace field_visibility

namespace reference_return {

struct first_field {
  template <typename T>
  auto operator()(omni::binding_t<T> binding) const
    -> decltype((binding.value.first)) {
    return binding.value.first;
  }
};

} // namespace reference_return

inline namespace v1 {

struct inline_namespace_record {
  int value;
};

struct inline_namespace_parent {
  struct nested {
    int value;
  };
};

} // namespace v1
} // namespace interface_test

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

TEST(type_names, inline_namespace_record_qualified_type_name) {
  using interface_test::inline_namespace_record;

  EXPECT_EQ("interface_test::v1::inline_namespace_record",
    omni::reflected_call(interface_test::qualified_type_name,
      omni::type_t<inline_namespace_record>{}));
}

TEST(type_names, inline_namespace_nested_record_qualified_type_name) {
  using nested = interface_test::inline_namespace_parent::nested;

  EXPECT_EQ("interface_test::v1::inline_namespace_parent::nested",
    omni::reflected_call(interface_test::qualified_type_name,
      omni::type_t<nested>{}));
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

TEST(fields, own_field_hides_inherited_fields) {
  using namespace interface_test::field_visibility;

  derived_hides_bases input{};
  input.left_base::shared = 1;
  input.left = 2;
  input.right_base::shared = 3;
  input.right = 4;
  input.shared = 5;
  input.own = 6;

  static const std::vector<std::pair<std::string, int>> k_expected{
    {"left", 2},
    {"right", 4},
    {"shared", 5},
    {"own", 6},
  };

  EXPECT_EQ(k_expected, omni::reflected_call(name_values{}, input));
}

TEST(fields, base_field_hides_its_inherited_field) {
  using namespace interface_test::field_visibility;

  base_hides_base input{};
  input.root_base::shared = 1;
  input.root = 2;
  input.middle_base::shared = 3;
  input.middle = 4;
  input.own = 5;

  static const std::vector<std::pair<std::string, int>> k_expected{
    {"root", 2},
    {"shared", 3},
    {"middle", 4},
    {"own", 5},
  };

  EXPECT_EQ(k_expected, omni::reflected_call(name_values{}, input));
}

TEST(fields, namespaced_field_type_names) {
  using interface_test::nested::namespaced_field_types_t;
  namespace f = interface_test::fields;

  static const std::vector<std::string> k_expected{
    "namespaced_record_t",
    "namespaced_enum_t",
    "parent_record_t::nested_record_t",
  };

  value_categories_test<namespaced_field_types_t>(k_expected,
    f::record_type_field_type_names);
}

TEST(fields, namespaced_field_qualified_type_names) {
  using interface_test::nested::namespaced_field_types_t;
  namespace f = interface_test::fields;

  static const std::vector<std::string> k_expected{
    "interface_test::nested::namespaced_record_t",
    "interface_test::nested::namespaced_enum_t",
    "interface_test::nested::parent_record_t::nested_record_t",
  };

  value_categories_test<namespaced_field_types_t>(k_expected,
    f::record_type_field_qualified_type_names);
}

TEST(fields, fully_qualified_duplicate_leaf_field_type_names) {
  using interface_test::nested::duplicate_leaf_field_types_t;
  namespace f = interface_test::fields;

  static const std::vector<std::string> k_expected{
    "duplicate_name_t",
    "duplicate_name_t",
  };

  value_categories_test<duplicate_leaf_field_types_t>(k_expected,
    f::record_type_field_type_names);
}

TEST(fields, fully_qualified_duplicate_leaf_field_qualified_type_names) {
  using interface_test::nested::duplicate_leaf_field_types_t;
  namespace f = interface_test::fields;

  static const std::vector<std::string> k_expected{
    "interface_test::nested::left::duplicate_name_t",
    "interface_test::nested::right::duplicate_name_t",
  };

  value_categories_test<duplicate_leaf_field_types_t>(k_expected,
    f::record_type_field_qualified_type_names);
}

TEST(fields, sized_integer_field_types) {
  using interface_test::nested::sized_integer_field_types_t;
  namespace f = interface_test::fields;

  static const std::vector<std::string> k_expected{
    "uint16_t",
    "int32_t",
    "uint64_t *",
    "int16_t **",
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

TEST(fields, sized_integer_field_qualified_type_names) {
  using interface_test::nested::sized_integer_field_types_t;
  namespace f = interface_test::fields;

  // Qualified field type names preserve the source declaration spelling.
  static const std::vector<std::string> k_expected{
    "std::uint16_t",
    "std::int32_t",
    "std::uint64_t *",
    "std::int16_t **",
  };

  value_categories_test<sized_integer_field_types_t>(k_expected,
    f::record_type_field_qualified_type_names);
}

TEST(fields, field_qualification_metadata) {
  using interface_test::field_qualification_record_t;
  namespace fq = interface_test::field_qualification;

  static const std::vector<std::string> k_expected{
    "normal:const=false:mutable=false",
    "constant:const=true:mutable=false",
    "cache:const=false:mutable=true",
    "flags:const=false:mutable=false",
  };

  EXPECT_EQ(k_expected,
    omni::reflected_call(fq::field_flags_from_meta,
      omni::type_t<field_qualification_record_t>{}));

  field_qualification_record_t value{1, 2, 3, 4};
  EXPECT_EQ(k_expected,
    omni::reflected_call(fq::field_flags_from_binding, value));
}

TEST(fields, field_qualification_qualified_type_names) {
  using interface_test::field_qualification_record_t;
  namespace f = interface_test::fields;

  static const std::vector<std::string> k_expected{
    "int",
    "int",
    "int",
    "unsigned int",
  };

  value_categories_test<field_qualification_record_t>(k_expected,
    f::record_type_field_qualified_type_names);
}

TEST(fields, field_meta_write_availability) {
  using interface_test::field_qualification_record_t;
  namespace fq = interface_test::field_qualification;

  static const std::vector<std::string> k_expected{
    "normal:mutable_owner=true:const_owner=false",
    "constant:mutable_owner=false:const_owner=false",
    "cache:mutable_owner=true:const_owner=true",
    "flags:mutable_owner=true:const_owner=false",
  };

  EXPECT_EQ(k_expected,
    omni::reflected_call(fq::meta_write_availability,
      omni::type_t<field_qualification_record_t>{}));
}

TEST(fields, field_binding_write_availability) {
  using interface_test::field_qualification_record_t;
  namespace fq = interface_test::field_qualification;

  static const std::vector<std::string> k_mutable_expected{
    "normal:set_value=true",
    "constant:set_value=false",
    "cache:set_value=true",
    "flags:set_value=true",
  };

  static const std::vector<std::string> k_const_expected{
    "normal:set_value=false",
    "constant:set_value=false",
    "cache:set_value=true",
    "flags:set_value=false",
  };

  field_qualification_record_t value{1, 2, 3, 4};
  const field_qualification_record_t const_value{1, 2, 3, 4};

  EXPECT_EQ(k_mutable_expected,
    omni::reflected_call(fq::binding_write_availability, value));
  EXPECT_EQ(k_const_expected,
    omni::reflected_call(fq::binding_write_availability, const_value));
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

TEST(record_type_t, reflected_call_preserves_reference_return) {
  using interface_test::record_type_t;

  record_type_t input{7, {}};
  int &result = omni::reflected_call(
    interface_test::reference_return::first_field{}, input);
  result = 19;

  EXPECT_EQ(19, input.first);
}

TEST(record_type_t, reflected_rvalue_binding_can_be_named) {
  using interface_test::record_type_t;

  static const std::vector<std::string> k_expected{"815", "oceanic"};

  EXPECT_EQ(k_expected,
    omni::reflected_call(
      interface_test::inline_examples::rvalue_binding_can_be_named,
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

  const record_type_t owned =
    omni::reflected_call(fw::record_type_field_write_own_call_t{k_expected},
      record_type_t{});
  EXPECT_EQ_FIELDS(k_expected, owned);

#undef EXPECT_EQ_FIELDS
}
