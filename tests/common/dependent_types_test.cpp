#include "dependency_types.h"
#include "structs.h"

#include <omnirefl/reflected_call.hpp>

#include <gtest/gtest.h>

namespace {

TEST(dependency_resolved, as_field) {
  namespace dt = dependency_types;

  const dt::field_dep_level_1 v1{dt::resolved::as_field{1}};
  const std::string r1 =
    omni::reflected_call(dt::as_field::get_dependency_name, v1);
  EXPECT_EQ("field_dep_level_1::as_field:int", r1);

  const dt::field_dep_level_2 v2{
    dt::field_mid_level_2{dt::resolved::as_field_layer_2{2}}};
  const std::string r2 =
    omni::reflected_call(dt::as_field::get_dependency_name_layer_2, v2);
  EXPECT_EQ("field_dep_level_2::field_mid_level_2::as_field_layer_2:int", r2);
}

TEST(dependency_resolved, as_alias) {
  namespace dt = dependency_types;

  const dt::alias_dep_level_1 v1{};
  const std::string r1 =
    omni::reflected_call(dt::as_alias::get_dependency_name, v1);
  EXPECT_EQ("alias_dep_level_1::as_alias:int", r1);

  const dt::alias_dep_level_2 v2{};
  const std::string r2 =
    omni::reflected_call(dt::as_alias::get_dependency_name_layer_2, v2);
  EXPECT_EQ("alias_dep_level_2::alias_dep_mid_level_2::as_alias_layer_2:int",
    r2);
}

TEST(dependency_resolved, as_template_arg) {
  namespace dt = dependency_types;

  const dt::template_dep_level_1 v1{std::tuple<dt::resolved::as_template_arg>{
    dt::resolved::as_template_arg{1}}};
  const std::string r1 =
    omni::reflected_call(dt::as_template_arg::get_dependency_name, v1);
  EXPECT_EQ("template_dep_level_1::as_template_arg:int", r1);

  const dt::template_dep_level_2 v2{
    omni::compat::variant<std::tuple<dt::resolved::as_template_arg_layer_2>>{
      std::tuple<dt::resolved::as_template_arg_layer_2>{
        dt::resolved::as_template_arg_layer_2{2}}}};
  const std::string r2 =
    omni::reflected_call(dt::as_template_arg::get_dependency_name_layer_2, v2);
  EXPECT_EQ("template_dep_level_2::tuple::as_template_arg_layer_2:int", r2);

  const dt::mpark_template_dep_level_2 mpark_v2{
    mpark::variant<std::tuple<dt::resolved::as_template_arg_layer_2>>{
      std::tuple<dt::resolved::as_template_arg_layer_2>{
        dt::resolved::as_template_arg_layer_2{2}}}};

  const std::string mpark_r2 =
    omni::reflected_call(dt::as_template_arg::get_dependency_name_layer_2,
      mpark_v2);
  EXPECT_EQ("mpark_template_dep_level_2::tuple::as_template_arg_layer_2:int",
    mpark_r2);
}

TEST(dependency_resolved, as_inherited_struct) {
  namespace dt = dependency_types;

  const std::vector<std::string> k_expected_field_names{
    "base_field",
    "derived_field",
  };

  const dt::derived_struct derived{4, 8.15};
  const dt::as_inherited_struct::is_base_reflected_t<
    dt::resolved::as_inherited_struct>
    check{};

  EXPECT_TRUE(omni::reflected_call(check, derived));
  EXPECT_EQ(k_expected_field_names,
    omni::reflected_call(example_impl::print_field_names_simple, derived));
}

} // namespace
