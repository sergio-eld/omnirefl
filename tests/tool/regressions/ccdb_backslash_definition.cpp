#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <string_view>

#if !defined(OMNI_OPTION_TEXT)
#  error OMNI_OPTION_TEXT must be defined
#endif

static_assert("left\\right" == std::string_view{OMNI_OPTION_TEXT});

namespace regression_ccdb_backslash_definition {

struct record {
  int value;
};

} // namespace regression_ccdb_backslash_definition

// FIXME(high): ccdb_query rewrites every backslash in every compiler argument
// as a path separator, corrupting non-path macro values before omnirefl parses
// the translation unit.
TEST(regression, DISABLED_ccdb_preserves_literal_backslashes) {
  using regression_ccdb_backslash_definition::record;

  EXPECT_EQ(1,
    omni::reflected_call(
      [](auto binding) -> int { return binding.value.value; }, record{1}));
}
