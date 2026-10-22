// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Sergei Kolesnik

#if EXCLUSIONS_FIRST
#  include "exclusions_first.hpp"
#  include "scope_first.hpp"
#else
#  include "scope_first.hpp"
#  include "exclusions_first.hpp"
#endif

#include <gtest/gtest.h>

namespace exclusion_include_order {

struct root {
  dependency_exclusions::opaque child;
};

struct inspect {
  template <typename _M>
  std::size_t operator()(omni::record_meta_t<_M> meta) const {
#if !defined(OMNI_TOOL_RUN)
    EXPECT_FALSE(omni::is_reflected<dependency_exclusions::opaque>::value);
#endif

    return std::tuple_size<decltype(meta.public_fields())>::value;
  }
};

} // namespace exclusion_include_order

TEST(dependency_exclusions, headers_accept_both_exclusion_include_orders) {
  EXPECT_EQ(1U, omni::reflected_call(exclusion_include_order::inspect{},
    omni::type_t<exclusion_include_order::root>{}));

  // Guards are optional; ordinary compilation also accepts the no-op request.
  omni::tweaks::skip_metadata<dependency_exclusions::opaque>();

}
