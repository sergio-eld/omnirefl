// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Sergei Kolesnik

#include <gtest/gtest.h>
#include <omnirefl/reflected_scope.hpp>

#include "model.hpp"

namespace wrapped_exclusion {

struct inspect {
  template <typename _M>
  std::size_t operator()(omni::record_meta_t<_M> meta) const {
#if !defined(OMNI_TOOL_RUN)
    EXPECT_FALSE(omni::is_reflected<dependency_exclusions::opaque>::value);
#endif

    return std::tuple_size<decltype(meta.public_fields())>::value;
  }
};

template <typename T>
std::size_t reflect() {
  return omni::reflected_call(inspect{}, omni::type_t<T>{});
}

} // namespace wrapped_exclusion

#if defined(OMNI_TOOL_RUN)
#  include "exclusions.hpp"
#endif

struct wrapped_root {
  dependency_exclusions::opaque child;
};

TEST(dependency_exclusions, applies_through_an_earlier_defined_wrapper) {
  EXPECT_EQ(1U, wrapped_exclusion::reflect<wrapped_root>());

}
