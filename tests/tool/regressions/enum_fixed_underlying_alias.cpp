#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <cstdint>
#include <string_view>

namespace regression_enum_fixed_underlying_alias {

enum class state : std::uint64_t {
  ready,
  done,
};

} // namespace regression_enum_fixed_underlying_alias

// FIXME(high): the force-included generated header preserves the spelled
// std::uint64_t underlying type before the source's <cstdint> include is
// visible, making the enum forward declaration ill-formed.
TEST(regression, DISABLED_enum_forward_declaration_resolves_underlying_alias) {
  EXPECT_EQ("ready",
    omni::reflected_call(
      [](auto binding) -> std::string_view {
        return binding.enumerators()[0].second;
      },
      regression_enum_fixed_underlying_alias::state::ready));
}
