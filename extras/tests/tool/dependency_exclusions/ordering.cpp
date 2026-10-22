// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Sergei Kolesnik

#include <gtest/gtest.h>
#include <omnirefl/reflected_scope.hpp>

namespace exclusion_ordering {

struct discovered {
  int value;
};

struct before {
  discovered child;
};

struct excluded {
  int value;
};

struct inspect {
  template <typename _M>
  std::size_t operator()(omni::record_meta_t<_M> meta) const {
    // Test-only ad hoc: spell concrete fixture types to check exact metadata.
    // Guard non-dependent queries: generated traits are absent in tool runs.
#if !defined(OMNI_TOOL_RUN)
    EXPECT_FALSE(omni::is_reflected<exclusion_ordering::discovered>::value);
    EXPECT_FALSE(omni::is_reflected<exclusion_ordering::excluded>::value);
#endif

    return std::tuple_size<decltype(meta.public_fields())>::value;
  }
};

std::size_t reflect_before() {
  return omni::reflected_call(inspect{}, omni::type_t<before>{});
}

struct after {
  discovered existing;
  excluded ignored;
};

std::size_t reflect_after() {
  return omni::reflected_call(inspect{}, omni::type_t<after>{});
}

} // namespace exclusion_ordering

TEST(dependency_exclusions, applies_to_earlier_reflected_calls) {
  EXPECT_EQ(1U, exclusion_ordering::reflect_before());
  EXPECT_EQ(2U, exclusion_ordering::reflect_after());
}

#if defined(OMNI_TOOL_RUN)
extern template void
  omni::tweaks::skip_metadata<exclusion_ordering::discovered>();
extern template void
  omni::tweaks::skip_metadata<exclusion_ordering::excluded>();
#endif
