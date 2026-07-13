#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <cstdint>
#include <tuple>

namespace regression_packed_field_reference {

#if defined(_MSC_VER)
#  pragma pack(push, 1)
struct record {
  std::uint16_t value;
};
#  pragma pack(pop)
#else
struct __attribute__((packed)) record {
  std::uint16_t value;
};
#endif

} // namespace regression_packed_field_reference

// FIXME(high): generated non-bitfield accessors return references. Packed
// fields cannot safely provide those references and may produce a dangling one.
TEST(regression, DISABLED_packed_field_is_read_by_value) {
  regression_packed_field_reference::record input{53};

  EXPECT_EQ(53,
    omni::reflected_call(
      [](auto binding) -> std::uint16_t {
        return std::get<0>(binding.public_fields()).value();
      },
      input));
}
