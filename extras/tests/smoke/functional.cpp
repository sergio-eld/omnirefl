#include <omnirefl/functional.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

namespace fn_test {

TEST(fn_identity, preserves_reference_qualification) {
  int value = 42;
  const int &constant = value;

  auto &&lvalue = omni::fn::identity{}(value);
  auto &&read_only = omni::fn::identity{}(constant);
  auto &&rvalue = omni::fn::identity{}(std::move(value));

  static_assert(std::is_same<int &, decltype(lvalue)>::value,
    "identity must preserve lvalue references");
  static_assert(std::is_same<const int &, decltype(read_only)>::value,
    "identity must preserve const qualification");
  static_assert(std::is_same<int &&, decltype(rvalue)>::value,
    "identity must preserve rvalue references");
  EXPECT_EQ(&value, &lvalue);
  EXPECT_EQ(&value, &read_only);
  EXPECT_EQ(&value, &rvalue);
}

TEST(fn_identity, forwards_move_only_values) {
  auto value = omni::compat::make_unique<int>(42);
  const auto moved = omni::fn::identity{}(std::move(value));

  EXPECT_FALSE(value);
  EXPECT_TRUE(moved);
  if (moved) {
    EXPECT_EQ(42, *moved);
  }
}

TEST(fn_map, transforms_elements_into_a_tuple) {
  const auto tuple = std::make_tuple(1, 2, 3);

  const auto mapped = omni::fn::map([](int value) { return value * 2; }, tuple);

  EXPECT_EQ(std::make_tuple(2, 4, 6), mapped);
}

TEST(fn_map, defers_call_and_pipe_application) {
  const auto tuple = std::make_tuple(1, 2, 3);
  const auto double_value = omni::fn::map([](int value) { return value * 2; });

  const auto called = double_value(tuple);
  const auto piped = tuple | double_value;

  EXPECT_EQ(std::make_tuple(2, 4, 6), called);
  EXPECT_EQ(std::make_tuple(2, 4, 6), piped);
}

TEST(fn_foldl, sums_tuple_elements) {
  const auto tuple = std::make_tuple(1, 2, 3);

  const auto result =
    omni::fn::foldl([](int sum, int value) { return sum + value; }, 0, tuple);

  EXPECT_EQ(6, result);
}

TEST(fn_foldl, defers_call_and_pipe_application) {
  const auto tuple = std::make_tuple(1, 2, 3);
  const auto sum = omni::fn::foldl(
    [](int accumulated, int value) { return accumulated + value; },
    0);

  const auto called = sum(tuple);
  const auto piped = tuple | sum;

  EXPECT_EQ(6, called);
  EXPECT_EQ(6, piped);
}

} // namespace fn_test
