#include "dependency_types.h"
#include "gtest_include.h"
#include "structs.h"

#include <omnirefl/reflected_call.hpp>

#include <string>
#include <vector>

namespace {

TEST(example_value, field_names) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{"name", "count", "score"}),
    omni::reflected_call(dt::inspect::field_names,
      dt::example_value{"station", 4, 8.15}));
}

TEST(example_value, field_count) {
  namespace dt = dependency_types;

  EXPECT_EQ(std::size_t{3},
    omni::reflected_call(dt::inspect::field_count,
      dt::example_value{"station", 4, 8.15}));
}

TEST(example_value, field_indices) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::size_t>{0, 1, 2}),
    omni::reflected_call(dt::inspect::field_indices,
      dt::example_value{"station", 4, 8.15}));
}

TEST(example_value, reflected_name) {
  namespace dt = dependency_types;

  EXPECT_EQ("example_value",
    omni::reflected_call(dt::inspect::reflected_name,
      dt::example_value{"station", 4, 8.15}));
}

TEST(example_value, const_lvalue_route) {
  namespace dt = dependency_types;

  const dt::example_value value{"constant", 15, 16.23};
  EXPECT_EQ((std::vector<std::string>{"name", "count", "score"}),
    omni::reflected_call(dt::inspect::field_names, value));
}

TEST(example_value, rvalue_route) {
  namespace dt = dependency_types;

  EXPECT_EQ(std::size_t{3},
    omni::reflected_call(dt::inspect::field_count,
      dt::example_value{"temporary", 23, 42.0}));
}

TEST(field_dependency, level_1_value) {
  namespace dt = dependency_types;

  const dt::field_dep_level_1 v{dt::resolved::as_field{1}};
  const std::string r =
    omni::reflected_call(dt::as_field::get_dependency_name, v);
  EXPECT_EQ("field_dep_level_1::as_field:int", r);
}

TEST(field_dependency, level_1_field_names) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{"field_level_1"}),
    omni::reflected_call(dt::inspect::field_names,
      dt::field_dep_level_1{dt::resolved::as_field{1}}));
}

TEST(field_dependency, level_1_first_field_type) {
  namespace dt = dependency_types;

  EXPECT_EQ("as_field",
    omni::reflected_call(dt::inspect::first_field_type_name,
      dt::field_dep_level_1{dt::resolved::as_field{1}}));
}

TEST(field_dependency, level_2_value) {
  namespace dt = dependency_types;

  const dt::field_dep_level_2 v{
    dt::field_mid_level_2{dt::resolved::as_field_layer_2{2}},
  };
  const std::string r =
    omni::reflected_call(dt::as_field::get_dependency_name_layer_2, v);
  EXPECT_EQ("field_dep_level_2::field_mid_level_2::as_field_layer_2:int", r);
}

TEST(field_dependency, level_2_field_names) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{"field_level_2"}),
    omni::reflected_call(dt::inspect::field_names, dt::field_dep_level_2{}));
}

TEST(field_dependency, level_2_first_field_type) {
  namespace dt = dependency_types;

  EXPECT_EQ("field_mid_level_2",
    omni::reflected_call(dt::inspect::first_field_type_name,
      dt::field_dep_level_2{}));
}

TEST(field_dependency, two_fields_names) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{"first_dependency", "second_dependency"}),
    omni::reflected_call(dt::inspect::field_names, dt::field_dep_two_fields{}));
}

TEST(field_dependency, two_fields_dependency_types) {
  namespace dt = dependency_types;

  const dt::field_dep_two_fields v{};
  EXPECT_EQ("as_field",
    omni::reflected_call(dt::inspect::first_field_type_name, v));
  EXPECT_EQ("as_field_layer_2",
    omni::reflected_call(dt::inspect::second_field_type_name, v));
}

TEST(field_dependency, level_3_field_names) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{"field_level_3"}),
    omni::reflected_call(dt::inspect::field_names, dt::field_dep_level_3{}));
}

TEST(alias_dependency, level_1_value) {
  namespace dt = dependency_types;

  const dt::alias_dep_level_1 v{};
  const std::string r =
    omni::reflected_call(dt::as_alias::get_dependency_name, v);
  EXPECT_EQ("alias_dep_level_1::as_alias:int", r);
}

TEST(alias_dependency, level_1_has_no_public_fields) {
  namespace dt = dependency_types;

  EXPECT_EQ(std::vector<std::string>{},
    omni::reflected_call(dt::inspect::field_names, dt::alias_dep_level_1{}));
  EXPECT_EQ(std::size_t{0},
    omni::reflected_call(dt::inspect::field_count, dt::alias_dep_level_1{}));
}

TEST(alias_dependency, level_2_value) {
  namespace dt = dependency_types;

  const dt::alias_dep_level_2 v{};
  const std::string r =
    omni::reflected_call(dt::as_alias::get_dependency_name_layer_2, v);
  EXPECT_EQ("alias_dep_level_2::alias_dep_mid_level_2::as_alias_layer_2:int",
    r);
}

TEST(alias_dependency, level_2_has_no_public_fields) {
  namespace dt = dependency_types;

  EXPECT_EQ(std::vector<std::string>{},
    omni::reflected_call(dt::inspect::field_names, dt::alias_dep_level_2{}));
  EXPECT_EQ(std::size_t{0},
    omni::reflected_call(dt::inspect::field_count, dt::alias_dep_level_2{}));
}

TEST(alias_dependency, level_3_has_no_public_fields) {
  namespace dt = dependency_types;

  EXPECT_EQ(std::vector<std::string>{},
    omni::reflected_call(dt::inspect::field_names, dt::alias_dep_level_3{}));
  EXPECT_EQ(std::size_t{0},
    omni::reflected_call(dt::inspect::field_count, dt::alias_dep_level_3{}));
}

TEST(template_dependency, tuple_level_1_value) {
  namespace dt = dependency_types;

  const dt::template_dep_level_1 v{
    std::tuple<dt::resolved::as_template_arg>{
      dt::resolved::as_template_arg{1},
    },
  };
  const std::string r =
    omni::reflected_call(dt::as_template_arg::get_dependency_name, v);
  EXPECT_EQ("template_dep_level_1::as_template_arg:int", r);
}

TEST(template_dependency, tuple_level_1_field_names) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{"tpl_field_1"}),
    omni::reflected_call(dt::inspect::field_names, dt::template_dep_level_1{}));
}

TEST(template_dependency, tuple_level_2_value) {
  namespace dt = dependency_types;

  const dt::template_dep_level_2 v{
    omni::compat::variant<std::tuple<dt::resolved::as_template_arg_layer_2>>{
      std::tuple<dt::resolved::as_template_arg_layer_2>{
        dt::resolved::as_template_arg_layer_2{2},
      },
    },
  };
  const std::string r =
    omni::reflected_call(dt::as_template_arg::get_dependency_name_layer_2, v);
  EXPECT_EQ("template_dep_level_2::tuple::as_template_arg_layer_2:int", r);
}

TEST(template_dependency, mpark_tuple_level_2_value) {
  namespace dt = dependency_types;

  const dt::mpark_template_dep_level_2 v{
    mpark::variant<std::tuple<dt::resolved::as_template_arg_layer_2>>{
      std::tuple<dt::resolved::as_template_arg_layer_2>{
        dt::resolved::as_template_arg_layer_2{2},
      },
    },
  };
  const std::string r =
    omni::reflected_call(dt::as_template_arg::get_dependency_name_layer_2, v);
  EXPECT_EQ("mpark_template_dep_level_2::tuple::as_template_arg_layer_2:int",
    r);
}

// fixme: std::vector<T> dependency fields are not supported by source/header
// shared dependency collection yet. The backend currently special-cases tuple
// and variant only; this stays enabled as a regression route for dependency
// collection.

TEST(sequence_dependency, vector_value_type) {
  namespace dt = dependency_types;

  EXPECT_EQ("vector_dep_level_1::vector::as_sequence_vector",
    omni::reflected_call(dt::as_sequence_arg::get_vector_value_name,
      dt::vector_dep_level_1{}));
}

TEST(sequence_dependency, vector_field_names) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{"vector_field"}),
    omni::reflected_call(dt::inspect::field_names, dt::vector_dep_level_1{}));
}

TEST(sequence_dependency, tuple_first_value_type) {
  namespace dt = dependency_types;

  EXPECT_EQ("tuple_dep_two_values::tuple::as_sequence_tuple",
    omni::reflected_call(dt::as_sequence_arg::get_tuple_value_name,
      dt::tuple_dep_two_values{}));
}

TEST(sequence_dependency, tuple_second_value_type) {
  namespace dt = dependency_types;

  EXPECT_EQ("tuple_dep_two_values::tuple::as_field",
    omni::reflected_call(dt::as_sequence_arg::get_tuple_second_value_name,
      dt::tuple_dep_two_values{}));
}

TEST(sequence_dependency, tuple_field_names) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{"tuple_field"}),
    omni::reflected_call(dt::inspect::field_names, dt::tuple_dep_two_values{}));
}

TEST(sequence_dependency, compat_variant_value_type) {
  namespace dt = dependency_types;

  EXPECT_EQ("variant_dep_level_1::variant::as_sequence_variant",
    omni::reflected_call(dt::as_sequence_arg::get_variant_value_name,
      dt::variant_dep_level_1{}));
}

TEST(sequence_dependency, mpark_variant_value_type) {
  namespace dt = dependency_types;

  EXPECT_EQ("mpark_variant_dep_level_1::variant::as_sequence_variant",
    omni::reflected_call(dt::as_sequence_arg::get_variant_value_name,
      dt::mpark_variant_dep_level_1{}));
}

// fixme: same unsupported vector dependency route as vector_dep_level_1.

TEST(sequence_dependency, nested_vector_tuple_value_type) {
  namespace dt = dependency_types;

  EXPECT_EQ("nested_vector_tuple_dep::vector::tuple::as_template_arg_layer_2",
    omni::reflected_call(
      dt::as_sequence_arg::get_nested_vector_tuple_value_name,
      dt::nested_vector_tuple_dep{}));
}

TEST(sequence_dependency, nested_vector_tuple_field_names) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{"nested_field"}),
    omni::reflected_call(dt::inspect::field_names,
      dt::nested_vector_tuple_dep{}));
}

TEST(inheritance_dependency, base_is_reflected) {
  namespace dt = dependency_types;

  const dt::derived_struct derived{4, 8.15};
  const dt::as_inherited_struct::is_base_reflected_t<
    dt::resolved::as_inherited_struct>
    check{};

  EXPECT_TRUE(omni::reflected_call(check, derived));
}

TEST(inheritance_dependency, public_base_fields_are_flattened) {
  namespace dt = dependency_types;

  EXPECT_EQ(
    (std::vector<std::string>{
      "base_field",
      "derived_field",
    }),
    omni::reflected_call(dt::inspect::field_names,
      dt::derived_struct{4, 8.15}));
}

TEST(inheritance_dependency, public_base_indices) {
  namespace dt = dependency_types;

  EXPECT_EQ(
    (std::vector<std::size_t>{0, 0}),
    omni::reflected_call(dt::inspect::field_indices,
      dt::derived_struct{4, 8.15}));
}

TEST(inheritance_dependency, multi_base_fields_are_flattened) {
  namespace dt = dependency_types;

  EXPECT_EQ(
    (std::vector<std::string>{
      "base_field",
      "second_base_field",
      "own_field",
    }),
    omni::reflected_call(dt::inspect::field_names,
      dt::multi_base_derived{4, "second", 15}));
}

TEST(inheritance_dependency, multi_base_field_count) {
  namespace dt = dependency_types;

  EXPECT_EQ(std::size_t{3},
    omni::reflected_call(dt::inspect::field_count,
      dt::multi_base_derived{4, "second", 15}));
}

// FIXME: disabled: header mode reflects direct public base fields, but omits
// transitive public base fields inherited through that direct base.
TEST(inheritance_dependency, DISABLED_deep_base_fields_are_flattened) {
  namespace dt = dependency_types;

  EXPECT_EQ(
    (std::vector<std::string>{
      "base_field",
      "mid_field",
      "deep_field",
    }),
    omni::reflected_call(dt::inspect::field_names,
      dt::deep_derived{4, "middle", 8.15}));
}

TEST(inheritance_dependency, three_base_fields_are_flattened) {
  namespace dt = dependency_types;

  EXPECT_EQ(
    (std::vector<std::string>{
      "base_field",
      "second_base_field",
      "third_base_field",
      "own_field",
    }),
    omni::reflected_call(dt::inspect::field_names,
      dt::three_base_derived{4, "second", 8.15, "own"}));
}

TEST(inheritance_dependency, three_base_field_count) {
  namespace dt = dependency_types;

  EXPECT_EQ(std::size_t{4},
    omni::reflected_call(dt::inspect::field_count,
      dt::three_base_derived{4, "second", 8.15, "own"}));
}

// FIXME: does not compile in header mode: enum field dependencies that are also
// reflected directly can produce duplicate named/indexed _reflected
// specializations. The plain unscoped enum dependency also still needs indexed
// dependency metadata because it cannot be forward-declared.
//
// TEST(enum_dependency, enum_holder_field_names) {
//   namespace dt = dependency_types;
//
//   EXPECT_EQ((std::vector<std::string>{"plain", "scoped", "fixed"}),
//     omni::reflected_call(dt::inspect::field_names,
//       dt::enum_holder{
//         dt::plain_status_pending,
//         dt::scoped_status::idle,
//         dt::fixed_status_low,
//       }));
// }
//
// TEST(enum_dependency, enum_holder_first_type) {
//   namespace dt = dependency_types;
//
//   EXPECT_EQ("plain_status",
//     omni::reflected_call(dt::inspect::first_field_type_name,
//       dt::enum_holder{}));
// }
//
// TEST(enum_dependency, enum_holder_second_type) {
//   namespace dt = dependency_types;
//
//   EXPECT_EQ("scoped_status",
//     omni::reflected_call(dt::inspect::second_field_type_name,
//       dt::enum_holder{}));
// }

// FIXME: mixed_dependency_holder contains vector_dep_level_1, so it currently
// hits the same unsupported std::vector dependency route.
//
// TEST(mixed_dependency, holder_field_names) {
//   namespace dt = dependency_types;
//
//   EXPECT_EQ(
//     (std::vector<std::string>{
//       "value",
//       "field_dependency",
//       "vector_dependency",
//       "enum_dependency",
//     }),
//     omni::reflected_call(dt::inspect::field_names,
//       dt::mixed_dependency_holder{}));
// }
//
// TEST(mixed_dependency, holder_first_type) {
//   namespace dt = dependency_types;
//
//   EXPECT_EQ("example_value",
//     omni::reflected_call(dt::inspect::first_field_type_name,
//       dt::mixed_dependency_holder{}));
// }
//
// TEST(mixed_dependency, holder_field_count) {
//   namespace dt = dependency_types;
//
//   EXPECT_EQ(std::size_t{4},
//     omni::reflected_call(dt::inspect::field_count,
//       dt::mixed_dependency_holder{}));
// }

TEST(negative_public_only, non_public_fields_are_absent) {
  namespace dt = dependency_types;

  EXPECT_EQ((std::vector<std::string>{"visible"}),
    omni::reflected_call(dt::inspect::field_names,
      dt::non_public_fields{"visible", 4, 8.15}));
}

TEST(negative_public_only, non_public_field_count) {
  namespace dt = dependency_types;

  EXPECT_EQ(std::size_t{1},
    omni::reflected_call(dt::inspect::field_count,
      dt::non_public_fields{"visible", 4, 8.15}));
}

TEST(negative_public_only, private_base_fields_are_absent) {
  namespace dt = dependency_types;

  EXPECT_EQ(
    (std::vector<std::string>{
      "own_field",
    }),
    omni::reflected_call(dt::inspect::field_names,
      dt::private_base_derived{4, "own"}));
}

TEST(negative_public_only, protected_base_fields_are_absent) {
  namespace dt = dependency_types;

  EXPECT_EQ(
    (std::vector<std::string>{
      "own_field",
    }),
    omni::reflected_call(dt::inspect::field_names,
      dt::protected_base_derived{4, "own"}));
}

TEST(negative_public_only, mixed_access_keeps_only_public_members) {
  namespace dt = dependency_types;

  EXPECT_EQ(
    (std::vector<std::string>{
      "visible",
    }),
    omni::reflected_call(dt::inspect::field_names,
      dt::mixed_access_derived{1, "visible", 2, 3.5}));
}

} // namespace
