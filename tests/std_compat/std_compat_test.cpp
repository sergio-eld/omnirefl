#include "std_compat.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

using namespace std::string_view_literals;

TEST(std_compat, enumerate_preserves_references) {
  std::vector values{2, 4, 6};

  for (const auto &[index, value] : values | std_c::views::enumerate)
    values[index] = value + static_cast<int>(index);

  ASSERT_EQ(2, values[0]);
  ASSERT_EQ(5, values[1]);
  ASSERT_EQ(8, values[2]);
}

TEST(std_compat, enumerate_composes_with_tuple_views) {
  const std::vector values{2, 4, 6};

  auto selected = values //
    | std_c::views::enumerate //
    | std::views::filter(
      [](const auto &indexed) { return 0 != std::get<0>(indexed); }) //
    | std::views::values;
  auto value = selected.begin();

  ASSERT_NE(selected.end(), value);
  ASSERT_EQ(4, *value);
  ++value;
  ASSERT_NE(selected.end(), value);
  ASSERT_EQ(6, *value);
  ++value;
  ASSERT_EQ(selected.end(), value);
}

TEST(std_compat, enumerate_composes_with_reverse) {
  const std::vector values{2, 4, 6};
  const auto reversed =
    values | std_c::views::enumerate | std::views::reverse;
  const auto &[index, value] = *reversed.begin();

  ASSERT_EQ(std::size_t{2}, index);
  ASSERT_EQ(6, value);
}

TEST(std_compat, join_with_composes_with_ranges_to) {
  const std::array words{"one"sv, "two"sv};

  const std::string joined = words //
    | std_c::views::join_with("/"sv) //
    | std::ranges::to<std::string>();

  ASSERT_EQ("one/two", joined);
}
