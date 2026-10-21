// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Sergei Kolesnik

#include <gtest/gtest.h>
#include <omnirefl/reflected_scope.hpp>

namespace direct_exclusion {

struct record {
  int value;
};

template <typename T>
struct box {
  T value;
};

#if defined(OMNI_TOOL_RUN)
omni::exclude_dependencies<record, box<int>> exclusions;
#endif

struct inspect {
  template <typename _M>
  std::size_t operator()(omni::record_meta_t<_M> meta) const {
    return std::tuple_size<decltype(meta.public_fields())>::value;
  }
};

} // namespace direct_exclusion

TEST(dependency_exclusions, explicit_reflection_still_produces_metadata) {
  EXPECT_EQ(1U, omni::reflected_call(direct_exclusion::inspect{},
    omni::type_t<direct_exclusion::record>{}));
}

TEST(dependency_exclusions, explicitly_reflects_an_excluded_primary_template) {
  EXPECT_EQ(1U, omni::reflected_call(direct_exclusion::inspect{},
    omni::type_t<direct_exclusion::box<double>>{}));
}
