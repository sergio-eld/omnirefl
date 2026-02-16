
#include "gtest_include.h"
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

  // fixme: for header mode fix nested type. Can't generate forward declaration
  // there, but not really needed: for `struct wrestler::info_t` I can have the
  // forward declaration for `wrestler` and partial specialization for
  // `reflected_t<wrestler::info_t, T>` which will be delayed. The same can be
  // applied to unnamed types:
  // `reflected_t<decltype(std::declval<wrestler>().field), T>`.
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

} // namespace
