#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <array>
#include <cstdint>
#include <tuple>
#include <type_traits>

namespace packed_field_test {

#if defined(_MSC_VER)
#  pragma pack(push, 1)
struct record {
  char marker;
  std::uint16_t value;
  std::uint16_t values[2];
  std::uint16_t matrix[2][2];
};
#  pragma pack(pop)
#else
struct __attribute__((packed)) record {
  char marker;
  std::uint16_t value;
  std::uint16_t values[2];
  std::uint16_t matrix[2][2];
};
#endif

#pragma pack(push, 1)
struct reference_record {
  int &value;
};
#pragma pack(pop)

struct copied_values {
  std::uint16_t value;
  std::array<std::uint16_t, 2> values;
  std::array<std::array<std::uint16_t, 2>, 2> matrix;
};

struct write_value {
  template <typename T>
  copied_values operator()(omni::binding_t<T> binding) const {
    const auto fields = binding.public_fields();

    static_assert(
      std::is_reference<decltype(std::get<0>(fields).value())>::value,
      "alignment-one fields may retain reference access");

    auto value = std::get<1>(fields);

    static_assert(!std::is_reference<decltype(value.value())>::value,
      "packed fields must be read by value");
    static_assert(std::is_array<typename omni::compat::decay_t<
                    decltype(std::get<2>(fields))>::type>::value,
      "field metadata must retain the declared raw-array type");
    static_assert(std::is_same<decltype(std::get<2>(fields).value()),
                    std::array<std::uint16_t, 2>>::value,
      "packed arrays must be copied into aligned storage");
    static_assert(std::is_same<decltype(std::get<3>(fields).value()),
                    std::array<std::array<std::uint16_t, 2>, 2>>::value,
      "nested packed arrays must be copied recursively");
    static_assert(
      std::is_same<
        typename omni::compat::decay_t<decltype(std::get<3>(fields))>::type,
        std::uint16_t[2][2]>::value,
      "nested field metadata must retain the declared raw-array type");

    const auto values = std::get<2>(fields).value();
    const auto matrix = std::get<3>(fields).value();

    value.set_value(89);
    return {value.value(), values, matrix};
  }
};

write_value const static write{};

struct reference_address {
  template <typename T>
  int *operator()(omni::binding_t<T> binding) const {
    const auto field = std::get<0>(binding.public_fields());

    static_assert(std::is_reference<decltype(field.value())>::value,
      "reference fields must preserve the referent");
    return &field.value();
  }
};

reference_address const static address{};

} // namespace packed_field_test

TEST(fields, packed_field_is_read_by_value) {
  packed_field_test::record input{'x', 53, {1, 2}, {{3, 5}, {8, 13}}};

  const packed_field_test::copied_values copied =
    omni::reflected_call(packed_field_test::write, input);

  EXPECT_EQ(89, copied.value);
  EXPECT_EQ(1, copied.values[0]);
  EXPECT_EQ(2, copied.values[1]);
  EXPECT_EQ(3, copied.matrix[0][0]);
  EXPECT_EQ(5, copied.matrix[0][1]);
  EXPECT_EQ(8, copied.matrix[1][0]);
  EXPECT_EQ(13, copied.matrix[1][1]);
  EXPECT_EQ(89, input.value);
}

TEST(fields, packed_reference_field_preserves_referent) {
  int value = 53;
  packed_field_test::reference_record input{value};

  EXPECT_EQ(&value, omni::reflected_call(packed_field_test::address, input));
}
