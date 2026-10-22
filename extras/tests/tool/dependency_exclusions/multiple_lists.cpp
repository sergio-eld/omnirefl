// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Sergei Kolesnik

#include <gtest/gtest.h>
#include <omnirefl/reflected_scope.hpp>

namespace multiple_exclusions {

struct first {
  int value;
};

struct second {
  int value;
};

template <typename T>
struct view {
  T value;
};

struct retained {
  int value;
};

struct root {
  first a;
  second b;
  view<char> c;
  view<int> d;
  retained kept;
};

struct inspect {
  template <typename _M>
  std::size_t operator()(omni::record_meta_t<_M> meta) const {
    // Test-only ad hoc: spell concrete fixture types to check exact metadata.
    // Guard non-dependent queries: generated traits are absent in tool runs.
#if !defined(OMNI_TOOL_RUN)
    EXPECT_FALSE(omni::is_reflected<multiple_exclusions::first>::value);
    EXPECT_FALSE(omni::is_reflected<multiple_exclusions::second>::value);
    EXPECT_FALSE(omni::is_reflected<multiple_exclusions::view<char>>::value);
    EXPECT_FALSE(omni::is_reflected<multiple_exclusions::view<int>>::value);
    EXPECT_TRUE(omni::is_reflected<multiple_exclusions::retained>::value);
#endif

    return std::tuple_size<decltype(meta.public_fields())>::value;
  }
};

} // namespace multiple_exclusions

TEST(dependency_exclusions, combines_distinct_and_repeated_lists) {
  EXPECT_EQ(5U, omni::reflected_call(multiple_exclusions::inspect{},
    omni::type_t<multiple_exclusions::root>{}));
}

#if defined(OMNI_TOOL_RUN)
OMNI_SKIP_METADATA(multiple_exclusions::first, multiple_exclusions::second);
OMNI_SKIP_METADATA(multiple_exclusions::first);
OMNI_SKIP_METADATA(multiple_exclusions::view<char>);
OMNI_SKIP_METADATA(multiple_exclusions::view<int>);
#endif
