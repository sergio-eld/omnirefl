// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Sergei Kolesnik

#include <gtest/gtest.h>
#include <omnirefl/reflected_scope.hpp>

#include "model.hpp"

// Users may isolate instrumentation-only exclusions in a dedicated header.
#if defined(OMNI_TOOL_RUN)
#  include "exclusions.hpp"
#endif

namespace dependency_exclusions {

enum class retained_color { blue };

struct retained {
  int value;
};

struct root {
  opaque ignored;
  view<int> first;
  view<double> second;
  buffer<double, 3> storage;
  retained kept;
  excluded_color ignored_color;
  retained_color kept_color;
};

struct inspect {
  template <typename _M>
  std::size_t operator()(omni::record_meta_t<_M> meta) const {
    // Test-only ad hoc: spell concrete fixture types to check exact metadata.
    // Guard non-dependent queries: generated traits are absent in tool runs.
#if !defined(OMNI_TOOL_RUN)
    static_assert(!omni::is_reflected<opaque>::value,
      "an excluded concrete type has no dependency metadata");
    static_assert(!omni::is_reflected<hidden_leaf>::value,
      "excluded dependencies are not traversed");
    static_assert(!omni::is_reflected<view<int>>::value,
      "exclusion applies to other arguments of the primary template");
    static_assert(!omni::is_reflected<view<double>>::value,
      "every specialization shares the excluded primary template");
    static_assert(!omni::is_reflected<buffer<double, 3>>::value,
      "primary templates with non-type parameters can be excluded");
    static_assert(!omni::is_reflected<excluded_color>::value,
      "enum exclusions do not require record declarations");
    static_assert(omni::is_reflected<retained_color>::value,
      "other enum dependencies remain reflected");
    static_assert(omni::is_reflected<retained>::value,
      "other dependencies remain reflected");
#endif

    return std::tuple_size<decltype(meta.public_fields())>::value;
  }
};

} // namespace dependency_exclusions

TEST(dependency_exclusions, retains_fields_without_excluded_metadata) {
  EXPECT_EQ(7U, omni::reflected_call(dependency_exclusions::inspect{},
    omni::type_t<dependency_exclusions::root>{}));
}
