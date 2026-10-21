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

struct inspect {
  template <typename _M>
  std::size_t operator()(omni::record_meta_t<_M> meta) const {
    return std::tuple_size<decltype(meta.public_fields())>::value;
  }
};

std::size_t reflect_before() {
  return omni::reflected_call(inspect{}, omni::type_t<before>{});
}

struct excluded {
  int value;
};

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
#if defined(OMNI_TOOL_RUN)
  (void)omni::exclude_dependencies<exclusion_ordering::discovered,
    exclusion_ordering::excluded>{};
#else
  EXPECT_FALSE(omni::is_reflected<exclusion_ordering::discovered>::value);
  EXPECT_FALSE(omni::is_reflected<exclusion_ordering::excluded>::value);
#endif
}
