#include <gtest/gtest.h>

// FIXME(high): indexed generated-header reflection is currently disabled as a
// production test surface. The implementation relies on friend-injection
// indexes for non-forward-declarable local/unnamed types; those specializations
// are expensive for MSVC and currently exhaust compiler heap space in large
// test translation units. Keep indexed-type cases in this file until the
// implementation is either removed or redesigned.
#if 0 //< 1 to enable indexed generated-header tests.
#  include "index_regression.cpp"

// FIXME: generated-header reflection does not instrument supported dependency types reachable
// through reflected_call argument types when those dependency types cannot be
// forward-declared.
//
// TEST(write_values, in_cpp_local_unnamed_struct_from_std_variant_map) {
//   struct {
//     std::string name;
//     int count;
//     double score;
//   } p{};
//
//   std::map<std::string, std::variant<int, double, std::string>> from;
//   from["name"] = std::string{"standard"};
//   from["count"] = 64;
//   from["score"] = 11.5;
//   example_impl::from_std_map(from, p);
//
//   ASSERT_EQ("standard", p.name);
//   ASSERT_EQ(64, p.count);
//   ASSERT_EQ(11.5, p.score);
// }
#endif
