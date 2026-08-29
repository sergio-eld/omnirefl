#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>

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

template <typename Binding>
struct can_value {
  template <typename B>
  static auto test(int)
    -> decltype(std::declval<const B &>().value(), std::true_type{});

  template <typename>
  static std::false_type test(...);

  static const bool value = decltype(test<Binding>(0))::value;
};

template <typename Binding>
struct can_ref {
  template <typename B>
  static auto test(int)
    -> decltype(std::declval<const B &>().ref(), std::true_type{});

  template <typename>
  static std::false_type test(...);

  static const bool value = decltype(test<Binding>(0))::value;
};

struct write_value {
  template <typename T>
  std::uint16_t operator()(omni::binding_t<T> binding) const {
    const auto fields = binding.public_fields();
    typedef typename std::tuple_element<0, decltype(fields)>::type marker_field;
    typedef typename std::tuple_element<1, decltype(fields)>::type value_field;
    typedef typename std::tuple_element<2, decltype(fields)>::type array_field;
    typedef typename std::tuple_element<3, decltype(fields)>::type matrix_field;

    static_assert(marker_field::has_value_access(),
      "alignment-one fields must expose value access");
    static_assert(marker_field::has_reference_access(),
      "alignment-one fields may retain reference access");
    static_assert(value_field::has_value_access(),
      "packed scalar fields must expose copy access");
    static_assert(!value_field::has_reference_access(),
      "packed fields must be read by value");
    static_assert(!array_field::has_value_access(),
      "misaligned raw arrays must not expose value access");
    static_assert(!array_field::has_reference_access(),
      "misaligned raw arrays must not expose references");
    static_assert(!can_value<array_field>::value,
      "misaligned raw arrays must not change type when read");
    static_assert(!can_ref<array_field>::value,
      "misaligned raw arrays must not expose unsafe storage");
    static_assert(!matrix_field::has_value_access(),
      "nested misaligned raw arrays must not expose value access");
    static_assert(!can_value<matrix_field>::value,
      "nested misaligned raw arrays must not change type when read");
    static_assert(std::is_array<typename array_field::type>::value,
      "field metadata must retain the declared raw-array type");
    static_assert(
      std::is_same<typename matrix_field::type, std::uint16_t[2][2]>::value,
      "nested field metadata must retain the declared raw-array type");

    auto value = std::get<1>(fields);
    value.set_value(std::uint16_t{89});
    return std::move(value).value();
  }
};

write_value const static write{};

struct reference_address {
  template <typename T>
  int *operator()(omni::binding_t<T> binding) const {
    const auto field = std::get<0>(binding.public_fields());

    static_assert(field.has_reference_access(),
      "reference fields must preserve the referent");
    return &field.ref();
  }
};

reference_address const static address{};

} // namespace packed_field_test

TEST(fields, packed_field_is_read_by_value) {
  packed_field_test::record input{'x', 53, {1, 2}, {{3, 5}, {8, 13}}};

  const std::uint16_t copied =
    omni::reflected_call(packed_field_test::write, input);

  EXPECT_EQ(89, copied);
  EXPECT_EQ(89, input.value);
  EXPECT_EQ(1, input.values[0]);
  EXPECT_EQ(2, input.values[1]);
  EXPECT_EQ(3, input.matrix[0][0]);
  EXPECT_EQ(13, input.matrix[1][1]);
}

TEST(fields, packed_reference_field_preserves_referent) {
  int value = 53;
  packed_field_test::reference_record input{value};

  EXPECT_EQ(&value, omni::reflected_call(packed_field_test::address, input));
}
