
#include "tool/cli.hpp"
#include "tl/expected.hpp"

#include <gtest/gtest.h>

namespace tool::cli {
namespace {

// todo: implement

//
// struct test_case {
//   std::vector<std::string> args;
//   tl::expected<options, std::string> expected;
// };
//
// // todo: custom print
// struct cmp_options_eq {
//   const struct options &options;
//   bool operator==(const cmp_options_eq &other) const noexcept {
//     return //
//       std::visit(
//         [](const auto &lhs, const auto &rhs) -> bool {
//           if constexpr (!std::is_same_v<decltype(lhs), decltype(rhs)>)
//             return false;
//           else {
//             using type = std::decay_t<decltype(lhs)>;
//             if constexpr (std::is_same_v<target_mode, type>) {
//               return std::tie(lhs.exclude,
//                        lhs.sources,
//                        lhs.output_file,
//                        lhs.output_dir)
//                 == std::tie(rhs.exclude,
//                   rhs.sources,
//                   rhs.output_file,
//                   rhs.output_dir);
//             } else if constexpr (std::is_same_v<inplace_mode, type>) {
//               return std::tie(lhs.sources, lhs.output_dir)
//                 == std::tie(rhs.sources, rhs.output_dir);
//             } else {
//               static_assert([]<typename = type> { return false; }());
//               return false;
//             }
//           }
//         },
//         options.mode,
//         other.options.mode)
//       && std::tie(options.compilation_db_path,
//            options.resource_dir,
//            options.print_debug)
//       == std::tie(other.options.compilation_db_path,
//         other.options.resource_dir,
//         other.options.print_debug);
//   }
//
//     // Provide a friend overload.
//   void PrintTo(const cmp_options_eq& c, std::ostream *os) {
//     // absl::Format(&sink, "(%d, %d)", point.x, point.y);
//     // todo: print
//     *os << "fuck you asshole";
//   }
// };
//
// struct cli_parse: public testing::TestWithParam<test_case> {};
//
// TEST_P(cli_parse, parses_correctly) {
//   const auto &[input, expected] = GetParam();
//
//   // Convert vector<string> to argc/argv
//   std::vector<const char *> argv;
//   argv.reserve(input.size() + 1);
//   argv.push_back("test_executable");
//   for (const auto &arg : input)
//     argv.emplace_back(arg.c_str());
//
//   const tl::expected result = parse(argv.size(), argv.data());
//   if (expected.has_value()) {
//     ASSERT_TRUE(result) << result.error();
//     ASSERT_EQ(cmp_options_eq{*result}, cmp_options_eq{*expected});
//
//     return;
//   }
//
//   // todo: custom print
//   ASSERT_FALSE(result);
//   // ASSERT_EQ(expected
//
//   // todo: implement
//   // Validate results
//   // if (!result) {
//   //   ASSERT_FALSE(result) << "Expected error but got success";
//   //   EXPECT_EQ(result.error(), std::get<std::string>(expected));
//   //   return;
//   // }
//
//   // ASSERT_TRUE(result) << "Expected success but got error: "
//   //                     << (result ? "" : result.error());
//   // const auto &expected_opt = std::get<options>(expected);
//   // const auto &actual_opt = *result;
//
//   // // Validate common options
//   // EXPECT_EQ(actual_opt.compilation_db_path,
//   // expected_opt.compilation_db_path); EXPECT_EQ(actual_opt.resource_dir,
//   // expected_opt.resource_dir); EXPECT_EQ(actual_opt.print_debug,
//   // expected_opt.print_debug);
//
//   // // Validate mode-specific options
//   // if (std::holds_alternative<target_mode>(expected_opt.mode)) {
//   //   const auto &exp_tm = std::get<target_mode>(expected_opt.mode);
//   //   const auto *act_tm = std::get_if<target_mode>(&actual_opt.mode);
//   //   ASSERT_NE(act_tm, nullptr) << "Expected target mode";
//
//   //   EXPECT_EQ(act_tm->exclude, exp_tm.exclude);
//   //   EXPECT_EQ(act_tm->output_file, exp_tm.output_file);
//   //   EXPECT_EQ(act_tm->output_dir, exp_tm.output_dir);
//   //   EXPECT_EQ(act_tm->sources, exp_tm.sources);
//   // } else {
//   //   const auto &exp_im = std::get<inplace_mode>(expected_opt.mode);
//   //   const auto *act_im = std::get_if<inplace_mode>(&actual_opt.mode);
//   //   ASSERT_NE(act_im, nullptr) << "Expected inplace mode";
//
//   //   EXPECT_EQ(act_im->output_dir, exp_im.output_dir);
//   //   EXPECT_EQ(act_im->sources, exp_im.sources);
//   // }
// }

// INSTANTIATE_TEST_SUITE_P(CliCases,
//   cli_parse,
//   testing::Values(
//     // Valid target mode cases
//     test_case{
//       .args =
//         {
//           "-o=out.cpp",
//           "--resource-dir=/clang",
//         },
//       .expected =
//         options{
//           .mode =
//             target_mode{
//               .exclude = false,
//               .sources = {},
//               .output_file = "out.cpp",
//               .output_dir = ".",
//             },
//           .compilation_db_path = ".",
//           .resource_dir = "/clang",
//           .print_debug = false,
//         },
//     } // test_case{{"--exclude",
//             "-o",
//             "out.cpp",
//             "src1.cpp",
//             "src2.cpp",
//             "--resource-dir=/clang",
//             "-p=build"},
//   options{.mode = target_mode{.exclude = true,
//             .sources = {"src1.cpp",
//             "src2.cpp"}, .output_file =
//             "out.cpp", .output_dir = "."},
//     .compilation_db_path = "build",
//     .resource_dir = "/clang",
//     .print_debug = false}},

// // Valid inplace mode cases
// test_case{{"--inplace-mode", "src.cpp", "--resource-dir=/clang"},
//   options{.mode = inplace_mode{.sources = {"src.cpp"}, .output_dir =
//   "."},
//     .compilation_db_path = ".",
//     .resource_dir = "/clang",
//     .print_debug = false}},
// test_case{{"--inplace-mode", "--output-dir=gen", "src.cpp", "-p=build"},
//   options{.mode = inplace_mode{.sources = {"src.cpp"}, .output_dir =
//   "gen"},
//     .compilation_db_path = "build",
//     .resource_dir = "/clang",
//     .print_debug = false}},

// // Error cases
// test_case{{"--resource-dir=/clang"},
//   "Target mode requires output file (-o)"},
// test_case{{"--inplace-mode", "-o=out.cpp"},
//   "Target mode option -o used with --inplace-mode"},
// test_case{{"--inplace-mode"}, "Inplace mode requires source files"},
// test_case{{"-o=out.cpp", "--resource-dir=/clang", "--inplace-mode"},
//   "Target mode option -o used with --inplace-mode"},
// test_case{{"--exclude", "-o=out.cpp", "--inplace-mode"},
//   "Target mode option -o used with --inplace-mode"},
// test_case{{"--resource-dir=clang", "-p"},
//   "Option -p is missing required argument"}
// ));

} // namespace
} // namespace tool::cli
