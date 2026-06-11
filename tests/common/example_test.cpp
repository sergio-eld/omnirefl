
#include "gtest_include.h"
#include "source_mode_structs.h"
#include "structs.h" //< todo: move to a separate file

#include <omnirefl/reflected_call.hpp>

#include <mpark/variant.hpp>

namespace {

TEST(print_names, simple) {
  {
    const example_types::championship v{};
    const static std::vector<std::string> expected{
      "name",
      "title",
    };
    const std::vector<std::string> result =
      omni::reflected_call(example_impl::print_field_names_simple, v);
    EXPECT_EQ(expected, result);
  }

  {
    const example_types::wrestler v{};
    const static std::vector<std::string> expected{
      "name",
      "age",
      "catchphrase",
      "titles",
      "info",
    };
    const std::vector<std::string> result =
      omni::reflected_call(example_impl::print_field_names_simple, v);
    EXPECT_EQ(expected, result);
  }
}

// todo: remove repetition, 'deduce' suite name from the reflection impl object
// INSTANTIATE_REFLECTION_SUITE(print_field_names_recursive,
//   example_impl::print_field_names_recursive,
//   //> INPUTS
//   std::make_tuple( //
//     test_case(
//       std::vector<std::string>{
//         "name",
//         "title",
//       },
//       example_types::championship{})
//
//       ,
//     test_case(
//       std::vector<std::string>{
//         "name",
//         "age",
//         "catchphrase",
//
//         "titles",
//         "titles[].name",
//         "titles[].title",
//
//         // fixme: add suport in header-mode
//         "info",
//         "info.ring_name",
//         "info.signature_move",
//         "info.debut_year",
//       },
//       example_types::wrestler{})
//
//     // reflects member_sub (member field)
//     ,
//     test_case(
//       std::vector<std::string>{
//         "member",
//         "member.ms_str",
//         "member.ms_int",
//       },
//       example_types::with_member{})
//
//     // reflects vec_elem (vector::value_type)
//     ,
//     test_case(
//       std::vector<std::string>{
//         "vec",
//         "vec[].ve_str",
//       },
//       example_types::with_vec{})
//
//     // reflects map_key (map::key_type)
//     ,
//     test_case(
//       std::vector<std::string>{
//         "mp",
//       },
//       example_types::with_map_key{})
//
//     // reflects tuple_elem (std::tuple element)
//     ,
//     test_case(
//       std::vector<std::string>{
//         "tp",
//       },
//       example_types::with_tuple{})
//
//     // fixme: generated code fails to compile
//     // reflects variant_elem (std::variant alternative)
//     // ,
//     // test_case(
//     //   std::vector<std::string>{
//     //     "vr",
//     //   },
//     //   example_types::with_variant{})
//     //< INPUTS
//     ));

// INSTANTIATE_REFLECTION_SUITE(print_enum_type_info_suite,
//   example_impl::print_enum_type_info,
//   //> INPUTS
//   std::make_tuple(
//     // namespace-scope unscoped enum
//     test_case( //
//       example_impl::print_enum_type_info_t::result{
//         /*.type_info=*/{
//           /*.name=*/"ring_style",
//           /*.namespaces=*/{"example_types"},
//         },
//         /*.names =*/
//         {
//           "rs_technical",
//           "rs_high_flying",
//           "rs_power",
//         },
//       },
//       example_types::ring_style{})
//
//     // namespace-scope scoped enum
//     ,
//     test_case( //
//       example_impl::print_enum_type_info_t::result{
//         /*.type_info=*/{
//           /*.name=*/"brand",
//           /*.namespaces=*/{"example_types"},
//         },
//         /*.names =*/
//         {
//           "raw",
//           "smackdown",
//           "nxt",
//         },
//       },
//       example_types::brand{})
//
//     // dependency unscoped enum
//     ,
//     test_case( //
//       example_impl::print_enum_type_info_t::result{
//         /*.type_info=*/{
//           /*.name=*/"title_rank",
//           /*.namespaces=*/{"example_types", "dependency"},
//         },
//         /*.names =*/
//         {
//           "tr_midcard",
//           "tr_main_event",
//         },
//       },
//       example_types::dependency::title_rank{})
//
//     // dependency scoped enum
//     ,
//     test_case( //
//       example_impl::print_enum_type_info_t::result{
//         /*.type_info=*/{
//           /*.name=*/"promotion",
//           /*.namespaces=*/{"example_types", "dependency"},
//         },
//         /*.names =*/
//         {
//           "wwe",
//           "aew",
//           "njpw",
//         },
//       },
//       example_types::dependency::promotion{})
//     //< INPUTS
//     ));

// refactorme: use INSTANTIATE_REFLECTION_SUITE
// TEST(print_values, recursive) {
//   const example_types::wrestler v{
//     "John Cena",
//     47,
//     "You can't see me",
//     /*titles=*/
//     {
//       {"WWE Championship", "16-time champion"},
//       {"World Heavyweight Championship", "3-time champion"},
//       {"United States Championship", "5-time champion"},
//       {"Royal Rumble", "2-time winner"},
//       {"Money in the Bank", "1-time winner"},
//       {"Tag Team Championship", "4-time champion"},
//     },
//
//     /*info=*/
//     {
//       "John Cena",
//       "Attitude Adjustment",
//       2002,
//     },
//   };
//
//   const static std::vector<std::string> expected{
//     // basic Fields
//     "name: \"John Cena\"",
//     "age: 47",
//     "catchphrase: \"You can't see me\"",
//
//     // titles (vector elements)
//     "titles[0].name: \"WWE Championship\"",
//     "titles[0].title: \"16-time champion\"",
//     "titles[1].name: \"World Heavyweight Championship\"",
//     "titles[1].title: \"3-time champion\"",
//     "titles[2].name: \"United States Championship\"",
//     "titles[2].title: \"5-time champion\"",
//     "titles[3].name: \"Royal Rumble\"",
//     "titles[3].title: \"2-time winner\"",
//     "titles[4].name: \"Money in the Bank\"",
//     "titles[4].title: \"1-time winner\"",
//     "titles[5].name: \"Tag Team Championship\"",
//     "titles[5].title: \"4-time champion\"",
//
//     // nested info fields
//     "info.ring_name: \"John Cena\"",
//     "info.signature_move: \"Attitude Adjustment\"",
//     "info.debut_year: 2002",
//   };
//
//   std::vector<std::string> result;
//   omni::reflected_call(example_impl::print_field_values_recursive, v,
//   result); EXPECT_EQ(expected, result);
// }

// TEST(modify_fields, simple) {
//   const static std::map<std::string, std::string> input{
//     {"str", "oceanic"},
//     {"i", "815"},
//   };
//   const auto value = [](const std::string &k) { return input.find(k)->second;
//   }; example_types::settable output;
//   omni::reflected_call(example_impl::simple_from_map, output, input);
//   EXPECT_EQ(std::to_string(output.i), value("i"));
//   EXPECT_EQ(output.str, value("str"));
// }

TEST(source_mode_read_values, record) {
  source_mode::record value{};
  value.name = "record";
  value.count = 1;
  value.score = 1.5;

  ASSERT_EQ((std::vector<std::string>{"record", "1", "1.5"}),
    omni::reflected_call(source_mode_impl::field_values, value));
}

TEST(source_mode_read_values, scalar_pack) {
  source_mode::scalar_pack value{};
  value.enabled = true;
  value.code = 'r';
  value.level = 2u;
  value.label = "scalar";

  ASSERT_EQ((std::vector<std::string>{"true", "r", "2", "scalar"}),
    omni::reflected_call(source_mode_impl::field_values, value));
}

TEST(source_mode_read_values, derived_from_header) {
  source_mode::derived_from_header value{};
  value.in_header_field_0 = "base";
  value.in_header_field_1 = 3;
  value.in_header_field_2 = 3.5;
  value.derived_name = "derived";
  value.derived_count = 4;
  value.derived_score = 4.5;

  ASSERT_EQ((std::vector<std::string>{
              "base",
              "3",
              "3.5",
              "derived",
              "4",
              "4.5",
            }),
    omni::reflected_call(source_mode_impl::field_values, value));
}

TEST(source_mode_read_values, private_base_public_only) {
  source_mode::private_base value{};
  value.name = "private own";
  value.count = 5;
  value.score = 5.5;

  ASSERT_EQ((std::vector<std::string>{"private own", "5", "5.5"}),
    omni::reflected_call(source_mode_impl::field_values, value));
}

TEST(source_mode_read_values, mixed_access_public_only) {
  source_mode::mixed_access value{};
  value.name = "John Cena";
  value.count = 6;
  value.score = 6.5;

  ASSERT_EQ((std::vector<std::string>{"John Cena", "6", "6.5"}),
    omni::reflected_call(source_mode_impl::field_values, value));
}

TEST(source_mode_read_values, not_reflected_int_path) {
  ASSERT_TRUE(
    omni::reflected_call(source_mode_impl::maybe_field_names, 1)
      .empty());
}

// FIXME: source mode renders std::string reflected_call arguments as
// `std::std::string` in generated _call_impl definitions.
//
// TEST(source_mode_read_values, not_reflected_string_path) {
//   const std::string value = "plain";
//
//   ASSERT_TRUE(
//     omni::reflected_call(source_mode_impl::maybe_field_names, value)
//       .empty());
// }

TEST(source_mode_read_values, enum_names) {
  ASSERT_EQ((std::vector<std::string>{"alpha", "beta", "gamma"}),
    omni::reflected_call(source_mode_impl::enum_names,
      source_mode::scoped_enum{}));
}

TEST(source_mode_read_values, bitfield_values) {
  source_mode::bitfield_record value{5, 17, 7};

  ASSERT_EQ((std::vector<std::string>{"5", "17", "7"}),
    omni::reflected_call(source_mode_impl::field_values, value));
}

TEST(index_pollution, non_reflected_record_before_shared_record) {
  const source_mode::record value{};

  ASSERT_EQ((std::vector<std::string>{"name", "count", "score"}),
    omni::reflected_call(
      source_mode_impl::query_non_reflected_record_then_field_names,
      value));
}

TEST(index_pollution, non_reflected_enum_before_shared_record) {
  const source_mode::record value{};

  ASSERT_EQ((std::vector<std::string>{"name", "count", "score"}),
    omni::reflected_call(
      source_mode_impl::query_non_reflected_enum_then_field_names,
      value));
}

TEST(index_pollution, composed_non_reflected_type_before_shared_record) {
  const source_mode::record value{};

  ASSERT_EQ((std::vector<std::string>{"name", "count", "score"}),
    omni::reflected_call(
      source_mode_impl::query_composed_non_reflected_then_field_names,
      value));
}

TEST(index_pollution, mixed_non_reflected_probes_before_shared_record) {
  const source_mode::record value{};

  ASSERT_EQ((std::vector<std::string>{"name", "count", "score"}),
    omni::reflected_call(
      source_mode_impl::query_mixed_non_reflected_then_field_names,
      value));
}

TEST(source_mode_write_values, record_from_mpark_map) {
  source_mode::record value{};

  std::map<std::string, mpark::variant<int, double, std::string>> from;
  from["name"] = std::string{"mapped"};
  from["count"] = 8;
  from["score"] = 8.5;
  source_mode_impl::from_std_map(from, value);

  ASSERT_EQ("mapped", value.name);
  ASSERT_EQ(8, value.count);
  ASSERT_EQ(8.5, value.score);
}

TEST(source_mode_write_values, record_from_mpark_map_missing_field) {
  source_mode::record value{};
  value.name = "old";
  value.count = 9;
  value.score = 9.5;

  std::map<std::string, mpark::variant<int, double, std::string>> from;
  from["name"] = std::string{"partial"};
  from["score"] = 10.5;
  source_mode_impl::from_std_map(from, value);

  ASSERT_EQ("partial", value.name);
  ASSERT_EQ(9, value.count);
  ASSERT_EQ(10.5, value.score);
}

TEST(source_mode_write_values, record_from_mpark_map_wrong_type) {
  source_mode::record value{};
  value.name = "old";
  value.count = 10;
  value.score = 10.5;

  std::map<std::string, mpark::variant<int, double, std::string>> from;
  from["name"] = 11;
  from["count"] = std::string{"wrong"};
  from["score"] = 11.5;
  source_mode_impl::from_std_map(from, value);

  ASSERT_EQ("old", value.name);
  ASSERT_EQ(10, value.count);
  ASSERT_EQ(11.5, value.score);
}

TEST(source_mode_write_values, record_constructed_from_mpark_map) {
  std::map<std::string, mpark::variant<int, double, std::string>> from;
  from["name"] = std::string{"constructed"};
  from["count"] = 11;
  from["score"] = 11.5;

  const source_mode::record value =
    source_mode_impl::from_std_map<source_mode::record>(from);

  ASSERT_EQ("constructed", value.name);
  ASSERT_EQ(11, value.count);
  ASSERT_EQ(11.5, value.score);
}

TEST(source_mode_write_values, record_from_mpark_map_extra_keys) {
  source_mode::record value{};

  std::map<std::string, mpark::variant<int, double, std::string>> from;
  from["name"] = std::string{"extra"};
  from["count"] = 12;
  from["score"] = 12.5;
  from["ignored"] = std::string{"ignored"};
  source_mode_impl::from_std_map(from, value);

  ASSERT_EQ("extra", value.name);
  ASSERT_EQ(12, value.count);
  ASSERT_EQ(12.5, value.score);
}

TEST(source_mode_write_values, writable_scalar_pack_from_mpark_map) {
  source_mode::writable_scalar_pack value{};

  std::map<std::string, mpark::variant<char, unsigned, std::string>> from;
  from["code"] = 'w';
  from["level"] = 12u;
  from["label"] = std::string{"written"};
  source_mode_impl::from_std_map(from, value);

  ASSERT_EQ('w', value.code);
  ASSERT_EQ(12u, value.level);
  ASSERT_EQ("written", value.label);
}

TEST(source_mode_write_values,
  writable_scalar_pack_from_mpark_map_wrong_type) {
  source_mode::writable_scalar_pack value{};
  value.code = 'x';
  value.level = 13u;
  value.label = "old";

  std::map<std::string, mpark::variant<char, unsigned, std::string>> from;
  from["code"] = 14u;
  from["level"] = 'y';
  from["label"] = std::string{"valid"};
  source_mode_impl::from_std_map(from, value);

  ASSERT_EQ('x', value.code);
  ASSERT_EQ(13u, value.level);
  ASSERT_EQ("valid", value.label);
}

// FIXME: source mode renders bool inside template arguments as `_Bool`, which
// is not a valid C++ type name in generated reflected_call definitions.
//
// TEST(source_mode_write_values, scalar_pack_from_mpark_map) {
//   source_mode::scalar_pack value{};
//
//   std::map<std::string, mpark::variant<bool, char, unsigned, std::string>> from;
//   from["enabled"] = true;
//   from["code"] = 'w';
//   from["level"] = 12u;
//   from["label"] = std::string{"written"};
//   source_mode_impl::from_std_map(from, value);
//
//   ASSERT_EQ(true, value.enabled);
//   ASSERT_EQ('w', value.code);
//   ASSERT_EQ(12u, value.level);
//   ASSERT_EQ("written", value.label);
// }

TEST(source_mode_write_values, derived_from_header_from_mpark_map) {
  source_mode::derived_from_header value{};

  std::map<std::string, mpark::variant<int, double, std::string>> from;
  from["in_header_field_0"] = std::string{"base"};
  from["in_header_field_1"] = 15;
  from["in_header_field_2"] = 15.5;
  from["derived_name"] = std::string{"derived"};
  from["derived_count"] = 16;
  from["derived_score"] = 16.5;
  source_mode_impl::from_std_map(from, value);

  ASSERT_EQ("base", value.in_header_field_0);
  ASSERT_EQ(15, value.in_header_field_1);
  ASSERT_EQ(15.5, value.in_header_field_2);
  ASSERT_EQ("derived", value.derived_name);
  ASSERT_EQ(16, value.derived_count);
  ASSERT_EQ(16.5, value.derived_score);
}

TEST(source_mode_write_values, derived_from_record_from_mpark_map) {
  source_mode::derived_from_record value{};

  std::map<std::string, mpark::variant<int, double, std::string>> from;
  from["name"] = std::string{"record base"};
  from["count"] = 25;
  from["score"] = 25.5;
  from["derived_name"] = std::string{"record derived"};
  from["derived_count"] = 26;
  from["derived_score"] = 26.5;
  source_mode_impl::from_std_map(from, value);

  ASSERT_EQ("record base", value.name);
  ASSERT_EQ(25, value.count);
  ASSERT_EQ(25.5, value.score);
  ASSERT_EQ("record derived", value.derived_name);
  ASSERT_EQ(26, value.derived_count);
  ASSERT_EQ(26.5, value.derived_score);
}

TEST(source_mode_write_values, multi_base_from_mpark_map) {
  source_mode::multi_base value{};

  std::map<std::string, mpark::variant<int, double, std::string>> from;
  from["in_header_field_0"] = std::string{"header base"};
  from["in_header_field_1"] = 27;
  from["in_header_field_2"] = 27.5;
  from["name"] = std::string{"record base"};
  from["count"] = 28;
  from["score"] = 28.5;
  from["multi_name"] = std::string{"multi"};
  from["multi_count"] = 29;
  from["multi_score"] = 29.5;
  source_mode_impl::from_std_map(from, value);

  ASSERT_EQ("header base", value.in_header_field_0);
  ASSERT_EQ(27, value.in_header_field_1);
  ASSERT_EQ(27.5, value.in_header_field_2);
  ASSERT_EQ("record base", value.name);
  ASSERT_EQ(28, value.count);
  ASSERT_EQ(28.5, value.score);
  ASSERT_EQ("multi", value.multi_name);
  ASSERT_EQ(29, value.multi_count);
  ASSERT_EQ(29.5, value.multi_score);
}

TEST(source_mode_write_values, private_base_public_only_from_mpark_map) {
  source_mode::private_base value{};

  std::map<std::string, mpark::variant<int, double, std::string>> from;
  from["name"] = std::string{"private own"};
  from["count"] = 17;
  from["score"] = 17.5;
  source_mode_impl::from_std_map(from, value);

  ASSERT_EQ("private own", value.name);
  ASSERT_EQ(17, value.count);
  ASSERT_EQ(17.5, value.score);
}

TEST(source_mode_write_values, protected_base_public_only_from_mpark_map) {
  source_mode::protected_base value{};

  std::map<std::string, mpark::variant<int, double, std::string>> from;
  from["in_header_field_0"] = std::string{"not public"};
  from["in_header_field_1"] = 30;
  from["in_header_field_2"] = 30.5;
  from["name"] = std::string{"protected own"};
  from["count"] = 31;
  from["score"] = 31.5;
  source_mode_impl::from_std_map(from, value);

  ASSERT_EQ("protected own", value.name);
  ASSERT_EQ(31, value.count);
  ASSERT_EQ(31.5, value.score);
}

TEST(source_mode_write_values, mixed_access_public_only_from_mpark_map) {
  source_mode::mixed_access value{};

  std::map<std::string, mpark::variant<int, double, std::string>> from;
  from["hidden"] = std::string{"changed"};
  from["name"] = std::string{"John Cena"};
  from["count"] = 18;
  from["score"] = 18.5;
  source_mode_impl::from_std_map(from, value);

  ASSERT_EQ("you can't see me", value.hidden_value());
  ASSERT_EQ("John Cena", value.name);
  ASSERT_EQ(18, value.count);
  ASSERT_EQ(18.5, value.score);
}

TEST(source_mode_write_values, foo_bar_visitor) {
  source_mode::foo_bar_record value{1, 2, 3, "same"};

  omni::reflected_call(source_mode_impl::write_foo_bar, value);

  ASSERT_EQ(8, value.foo_count);
  ASSERT_EQ(15, value.bar_count);
  ASSERT_EQ(3, value.untouched_count);
  ASSERT_EQ("same", value.foo_name);
}

// FIXME: source mode does not generate reflected_call definitions reached only
// through recursive template instantiation inside nested map writes.
//
// TEST(source_mode_write_values, nested_mpark_variant_map) {
//   using nested_value = mpark::variant<int, double, std::string>;
//   using nested_map = std::map<std::string, nested_value>;
//   using value_type = mpark::variant<int, double, std::string, nested_map>;
//
//   source_mode::nested_holder value{};
//
//   nested_map nested;
//   nested["value"] = 19;
//   nested["tag"] = std::string{"nested"};
//
//   std::map<std::string, value_type> from;
//   from["nested"] = nested;
//   from["name"] = std::string{"holder"};
//   source_mode_impl::from_std_map(from, value);
//
//   ASSERT_EQ(19, value.nested.value);
//   ASSERT_EQ("nested", value.nested.tag);
//   ASSERT_EQ("holder", value.name);
// }
//
// TEST(source_mode_write_values, nested_mpark_flat_map) {
//   source_mode::nested_holder value{};
//
//   std::map<std::string, mpark::variant<int, double, std::string>> from;
//   from["value"] = 20;
//   from["tag"] = std::string{"flat"};
//   from["name"] = std::string{"holder flat"};
//   source_mode_impl::from_std_map(from, value);
//
//   ASSERT_EQ(20, value.nested.value);
//   ASSERT_EQ("flat", value.nested.tag);
//   ASSERT_EQ("holder flat", value.name);
// }
//
// TEST(source_mode_write_values, nested_mpark_wrong_nested_type) {
//   using nested_value = mpark::variant<int, double, std::string>;
//   using nested_map = std::map<std::string, nested_value>;
//   using value_type = mpark::variant<int, double, std::string, nested_map>;
//
//   source_mode::nested_holder value{};
//
//   std::map<std::string, value_type> from;
//   from["nested"] = std::string{"not nested"};
//   from["value"] = 21;
//   from["tag"] = std::string{"fallback"};
//   from["name"] = std::string{"holder fallback"};
//   source_mode_impl::from_std_map(from, value);
//
//   ASSERT_EQ(21, value.nested.value);
//   ASSERT_EQ("fallback", value.nested.tag);
//   ASSERT_EQ("holder fallback", value.name);
// }

TEST(source_mode_write_values, bitfields_from_mpark_map) {
  source_mode::bitfield_record value{};

  std::map<std::string, mpark::variant<unsigned, int>> from;
  from["flags"] = 5u;
  from["code"] = 17u;
  from["count"] = 22;
  source_mode_impl::from_std_map(from, value);

  ASSERT_EQ(5u, value.flags);
  ASSERT_EQ(17u, value.code);
  ASSERT_EQ(22, value.count);
}

TEST(source_mode_write_values, bitfields_from_mpark_map_missing_field) {
  source_mode::bitfield_record value{1, 2, 3};

  std::map<std::string, mpark::variant<unsigned, int>> from;
  from["code"] = 18u;
  source_mode_impl::from_std_map(from, value);

  ASSERT_EQ(1u, value.flags);
  ASSERT_EQ(18u, value.code);
  ASSERT_EQ(3, value.count);
}

TEST(source_mode_write_values, bitfields_from_mpark_map_wrong_type) {
  source_mode::bitfield_record value{1, 2, 3};

  std::map<std::string, mpark::variant<unsigned, int, std::string>> from;
  from["flags"] = std::string{"wrong"};
  from["code"] = 19u;
  from["count"] = std::string{"wrong"};
  source_mode_impl::from_std_map(from, value);

  ASSERT_EQ(1u, value.flags);
  ASSERT_EQ(19u, value.code);
  ASSERT_EQ(3, value.count);
}

#if defined CXX_STANDARD && 17 <= CXX_STANDARD
TEST(source_mode_write_values, record_from_std_variant_map) {
  source_mode::record value{};

  std::map<std::string, std::variant<int, double, std::string>> from;
  from["name"] = std::string{"std"};
  from["count"] = 23;
  from["score"] = 23.5;
  source_mode_impl::from_std_map(from, value);

  ASSERT_EQ("std", value.name);
  ASSERT_EQ(23, value.count);
  ASSERT_EQ(23.5, value.score);
}

TEST(source_mode_write_values, record_from_std_variant_map_extra_keys) {
  source_mode::record value{};

  std::map<std::string, std::variant<int, double, std::string>> from;
  from["name"] = std::string{"std extra"};
  from["count"] = 32;
  from["score"] = 32.5;
  from["ignored"] = std::string{"ignored"};
  source_mode_impl::from_std_map(from, value);

  ASSERT_EQ("std extra", value.name);
  ASSERT_EQ(32, value.count);
  ASSERT_EQ(32.5, value.score);
}

TEST(source_mode_write_values, writable_scalar_pack_from_std_variant_map) {
  source_mode::writable_scalar_pack value{};

  std::map<std::string, std::variant<char, unsigned, std::string>> from;
  from["code"] = 's';
  from["level"] = 24u;
  from["label"] = std::string{"std scalar"};
  source_mode_impl::from_std_map(from, value);

  ASSERT_EQ('s', value.code);
  ASSERT_EQ(24u, value.level);
  ASSERT_EQ("std scalar", value.label);
}

TEST(source_mode_write_values, bitfields_from_std_variant_map) {
  source_mode::bitfield_record value{};

  std::map<std::string, std::variant<unsigned, int>> from;
  from["flags"] = 6u;
  from["code"] = 20u;
  from["count"] = 33;
  source_mode_impl::from_std_map(from, value);

  ASSERT_EQ(6u, value.flags);
  ASSERT_EQ(20u, value.code);
  ASSERT_EQ(33, value.count);
}

// FIXME: source mode renders bool inside std::variant template arguments as
// `_Bool`, which is not a valid C++ type name in generated reflected_call
// definitions.
//
// TEST(source_mode_write_values, scalar_pack_from_std_variant_map) {
//   source_mode::scalar_pack value{};
//
//   std::map<std::string, std::variant<bool, char, unsigned, std::string>> from;
//   from["enabled"] = true;
//   from["code"] = 's';
//   from["level"] = 24u;
//   from["label"] = std::string{"std scalar"};
//   source_mode_impl::from_std_map(from, value);
//
//   ASSERT_EQ(true, value.enabled);
//   ASSERT_EQ('s', value.code);
//   ASSERT_EQ(24u, value.level);
//   ASSERT_EQ("std scalar", value.label);
// }

// FIXME: source mode does not generate reflected_call definitions reached only
// through recursive template instantiation inside nested map writes.
//
// TEST(source_mode_write_values, nested_std_variant_map) {
//   using nested_value = std::variant<int, double, std::string>;
//   using nested_map = std::map<std::string, nested_value>;
//   using value_type = std::variant<int, double, std::string, nested_map>;
//
//   source_mode::nested_holder value{};
//
//   nested_map nested;
//   nested["value"] = 25;
//   nested["tag"] = std::string{"std nested"};
//
//   std::map<std::string, value_type> from;
//   from["nested"] = nested;
//   from["name"] = std::string{"std holder"};
//   source_mode_impl::from_std_map(from, value);
//
//   ASSERT_EQ(25, value.nested.value);
//   ASSERT_EQ("std nested", value.nested.tag);
//   ASSERT_EQ("std holder", value.name);
// }
#endif

} // namespace
