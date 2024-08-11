#include "foo.h"

#include <omnirefl/refl.h>

#include <gtest/gtest.h>

namespace {

// fixme: `foo` is a dependent field in `bar`, but as of this writing the implementation doesn't
// detect it and `foo` serialization is not generated...
[[maybe_unused]] void _force_gen() {
  (void)omni::deserialize.to<foo>(ryml::ConstNodeRef{});
}
// todo: use benchmarking
// todo: use codogen
TEST(ryml, deserialize) {
  const auto res = omni::deserialize.to<bar>(ryml::parse_in_arena( //
    R"({"d": 23.42, "f": {"a": "oceanic", "b": 815}})"));
  ASSERT_TRUE(res) << res.error();
  const bar &b = *res;
  ASSERT_EQ(b.d, 23.42);
  ASSERT_EQ(b.f.a, "oceanic");
  ASSERT_EQ(b.f.b, 815);
}

// todo: test and benchmark serialization
//   - same type (from `T` to `T`)
//   - fundamental types
//   - containers (to a reasonable extent)
//   - local and unnamed structs (need to implement)

// int main() {
//   // todo: use `omni::deserialize` for fundamental types
//   {
//     bool v;
//     if (const auto res = deserialize(ryml::parse_in_arena(R"({"oceanic": 815})")["oceanic"], v);
//         !res)
//       std::cout << res.error() << '\n';
//     else {
//       std::cout << v << '\n' << *res.value() << '\n';
//     }
//   }
//
//   {
//     std::vector<int> v;
//     if (const auto res =
//           deserialize(ryml::parse_in_arena(R"({"oceanic": [4, 8, 15, 16, 23, 42]})")["oceanic"],
//           v);
//         !res)
//       std::cout << res.error() << '\n';
//     else {
//       for (int i : v)
//         std::cout << i << ' ';
//       std::cout << '\n';
//     }
//   }
//
//   {
//     struct {
//       std::string a;
//       int b;
//     } oceanic;
//
//     // todo: meta stuff
//     object<int, std::string> _oceanic{{
//       {"b", &oceanic.b},
//       {"a", &oceanic.a},
//     }};
//
//     if (const auto res = deserialize(
//           // todo: handle invalid case `{"oceanic": {108, 815}}`, parser doesn't catch that
//           ryml::parse_in_arena(R"({"a": "oceanic", "b": 815})"),
//           _oceanic);
//         !res)
//       std::cout << res.error() << '\n';
//     else {
//       std::cout << oceanic.a << ' ' << oceanic.b << '\n';
//     }
//   }
//
//   return 0;
// }
} // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  // todo: ::benchmark::Initialize(&argc, argv);
  return RUN_ALL_TESTS();
}
