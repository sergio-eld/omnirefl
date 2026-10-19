#include <gtest/gtest.h>
#include <omnirefl/reflected_scope.hpp>

#include <string_view>

#if !defined(OMNI_OPTION_TEXT)
#  error OMNI_OPTION_TEXT must be defined
#endif

static_assert("left\\right" == std::string_view{OMNI_OPTION_TEXT});

namespace ccdb_backslash_definition {

struct record {
  int number;
};

} // namespace ccdb_backslash_definition

TEST(ccdb_query, preserves_literal_backslashes) {
  using ccdb_backslash_definition::record;

  EXPECT_EQ(1,
    omni::reflected_call(
      [](auto binding) -> int { return binding.ref().number; },
      record{1}));
}
