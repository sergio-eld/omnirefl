#include <gtest/gtest.h>
#include <omnirefl/reflected_scope.hpp>

#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

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
  std::uint16_t operator()(omni::record_binding_t<T> binding) const {
    const auto fields = binding.accessible_data_members();
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
  int *operator()(omni::record_binding_t<T> binding) const {
    const auto field = std::get<0>(binding.accessible_data_members());

    static_assert(field.has_reference_access(),
      "reference fields must preserve the referent");
    return &field.ref();
  }
};

reference_address const static address{};

struct bitfield_record {
  int signed_value : 4;
  unsigned : 0;
  mutable unsigned count : 3;
  volatile unsigned flag : 1;
  int ordinary;
  int &reference;
  static int shared;
};

struct inherited_bitfields: bitfield_record {};

struct bitfield_flags {
  template <typename... Field>
  std::vector<bool> operator()(Field...) const {
    // Require a constant expression, including in C++11.
    return {std::integral_constant<bool, Field::is_bitfield()>::value...};
  }
};

struct inspect_bitfield_metadata {
  template <typename _M>
  std::vector<bool> operator()(omni::record_meta_t<_M> record) const {
    return omni::compat::apply(bitfield_flags{},
      record.accessible_data_members());
  }
};

struct inspect_bitfield_bindings {
  template <typename T>
  std::vector<bool> operator()(omni::record_binding_t<T> record) const {
    return omni::compat::apply(bitfield_flags{},
      record.accessible_data_members());
  }
};

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

TEST(fields, bitfield_query_identifies_declarations_and_inherited_members) {
  const std::vector<bool> expected{true, true, true, false, false, false};

  EXPECT_EQ(expected,
    omni::reflected_call(packed_field_test::inspect_bitfield_metadata{},
      omni::type_t<packed_field_test::bitfield_record>{}));
  EXPECT_EQ(expected,
    omni::reflected_call(packed_field_test::inspect_bitfield_metadata{},
      omni::type_t<packed_field_test::inherited_bitfields>{}));
}

TEST(fields, bitfield_query_is_inherited_by_bindings) {
  int value = 7;
  packed_field_test::bitfield_record input{3, 5, 1, 11, value};

  EXPECT_EQ((std::vector<bool>{true, true, true, false, false, false}),
    omni::reflected_call(packed_field_test::inspect_bitfield_bindings{},
      input));
}

TEST(fields, packed_members_are_not_bitfields) {
  EXPECT_EQ((std::vector<bool>{false, false, false, false}),
    omni::reflected_call(packed_field_test::inspect_bitfield_metadata{},
      omni::type_t<packed_field_test::record>{}));
}
