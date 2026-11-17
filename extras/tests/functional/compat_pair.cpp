#include <omnirefl/compat/pair.hpp>
#include <omnirefl/compat.hpp>

#include <gtest/gtest.h>

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(__GNUC__) && !defined(__clang__)
// GCC warns about GTest's internal conditionals in unbraced assertion guards.
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdangling-else"
#endif

namespace {

enum class state { idle, active };

struct sum {
  constexpr int operator()(int first, int second) const {
    return first + second;
  }
};

struct sum_three {
  constexpr int operator()(int first, int second, int third) const {
    return first + second + third;
  }
};

struct non_default {
  explicit non_default(int) {}
};

TEST(compat_pair, constructs_an_enum_table_in_constant_evaluation) {
  constexpr std::array<omni::compat::pair<state, const char *>, 2> entries{{
    {state::idle, "idle"},
    {state::active, "active"},
  }};
  EXPECT_EQ(state::active, entries[1].first);
  EXPECT_STREQ("active", entries[1].second);
}

TEST(compat_pair, constructs_standard_containers_from_pair_ranges) {
  constexpr std::array<omni::compat::pair<state, const char *>, 2> entries{{
    {state::idle, "idle"},
    {state::active, "active"},
  }};
  const std::map<state, std::string> names{entries.begin(), entries.end()};
  const std::vector<std::pair<state, std::string>> values{
    entries.begin(), entries.end()};

  EXPECT_EQ("active", names.at(state::active));
  EXPECT_EQ((std::vector<std::pair<state, std::string>>{
    {state::idle, "idle"},
    {state::active, "active"},
  }), values);
}

TEST(compat_pair, converts_owned_elements_to_a_standard_pair) {
  const std::pair<std::unique_ptr<int>, std::string> value =
    omni::compat::make_pair(omni::compat::make_unique<int>(42), "answer");

  EXPECT_EQ("answer", value.second);
  EXPECT_TRUE(value.first);
  if (value.first)
    EXPECT_EQ(42, *value.first);
}

TEST(compat_pair, makes_a_pair_in_constant_evaluation) {
  constexpr auto value = omni::compat::make_pair(state::active, "active");
  constexpr auto first = omni::compat::get<0>(value);
  constexpr auto second = omni::compat::get<1>(value);

  EXPECT_EQ(state::active, first);
  EXPECT_STREQ("active", second);
  EXPECT_TRUE((std::is_same<const char *,
    decltype(value)::second_type>::value));
}

TEST(compat_pair, unwraps_reference_wrappers_when_making_a_pair) {
  int source = 7;
  auto value = omni::compat::make_pair(std::ref(source), 8);

  value.first = 42;

  EXPECT_EQ(42, source);
  EXPECT_EQ(8, value.second);
}

TEST(compat_pair, copies_lvalues_and_moves_owned_arguments) {
  int source = 7;
  auto value = omni::compat::make_pair(source,
    omni::compat::make_unique<int>(42));

  value.first = 8;

  EXPECT_EQ(7, source);
  EXPECT_EQ(8, value.first);
  EXPECT_TRUE(value.second);
  if (value.second)
    EXPECT_EQ(42, *value.second);
}

TEST(compat_pair, value_initializes_both_elements) {
  constexpr omni::compat::pair<int, int> value{};

  EXPECT_EQ(0, value.first);
  EXPECT_EQ(0, value.second);
}

TEST(compat_pair, forwards_move_only_elements) {
  omni::compat::pair<std::unique_ptr<int>, std::string> value{
    omni::compat::make_unique<int>(42), "answer"};

  auto moved = std::move(value);

  EXPECT_FALSE(value.first);
  EXPECT_EQ("answer", moved.second);
  EXPECT_TRUE(moved.first);
  if (moved.first)
    EXPECT_EQ(42, *moved.first);
}

TEST(compat_pair, converts_element_types) {
  constexpr omni::compat::pair<int, int> source{20, 22};
  constexpr omni::compat::pair<long, long> copied{source};
  const omni::compat::pair<std::unique_ptr<int>, int> moved{
    omni::compat::pair<std::unique_ptr<int>, short>{
      omni::compat::make_unique<int>(42), short{7}}};

  EXPECT_EQ(20L, copied.first);
  EXPECT_EQ(22L, copied.second);
  EXPECT_EQ(7, moved.second);
  EXPECT_TRUE(moved.first);
  if (moved.first)
    EXPECT_EQ(42, *moved.first);
}

TEST(compat_pair, supports_adl_tuple_access_and_mutation) {
  omni::compat::pair<int, std::string> value{7, "before"};
  using std::get;

  get<0>(value) = 42;
  get<1>(value) = "after";

  EXPECT_EQ(42, value.first);
  EXPECT_EQ("after", value.second);
  EXPECT_EQ(2U, std::tuple_size<decltype(value)>::value);
  EXPECT_TRUE((std::is_same<int,
    std::tuple_element<0, decltype(value)>::type>::value));
  EXPECT_TRUE((std::is_same<std::string,
    std::tuple_element<1, decltype(value)>::type>::value));
}

TEST(compat_pair, preserves_reference_elements_through_const_access) {
  int first = 7;
  int second = 8;
  const omni::compat::pair<int &, int &> value{first, second};

  omni::compat::get<0>(value) = 42;
  omni::compat::get<1>(std::move(value)) = 43;

  EXPECT_EQ(42, first);
  EXPECT_EQ(43, second);
}

TEST(compat_pair, preserves_all_access_categories) {
  using pair = omni::compat::pair<int, long>;

  EXPECT_TRUE((std::is_same<int &,
    decltype(omni::compat::get<0>(std::declval<pair &>()))>::value));
  EXPECT_TRUE((std::is_same<const int &,
    decltype(omni::compat::get<0>(std::declval<const pair &>()))>::value));
  EXPECT_TRUE((std::is_same<long &&,
    decltype(omni::compat::get<1>(std::declval<pair &&>()))>::value));
// The C++11 fallback supplies const-rvalue get missing in older libstdc++.
#if 201402L > OMNI_CPLUSPLUS
  EXPECT_TRUE((std::is_same<const long &&,
    decltype(omni::compat::get<1>(std::declval<const pair &&>()))>::value));
#endif
}

TEST(compat_pair, applies_a_callable_in_constant_evaluation) {
  constexpr omni::compat::pair<int, int> value{20, 22};
  constexpr auto result = omni::compat::apply(sum{}, value);

  EXPECT_EQ(42, result);
}

TEST(compat_pair, leaves_other_tuple_arities_accessible) {
  const std::tuple<int, int, int> values{4, 8, 30};

  EXPECT_EQ(42, omni::compat::apply(sum_three{}, values));
}

TEST(compat_pair, constrains_construction_to_supported_elements) {
  EXPECT_FALSE((std::is_default_constructible<
    omni::compat::pair<non_default, int>>::value));
  EXPECT_FALSE((std::is_constructible<omni::compat::pair<int, int>,
    const char *, int>::value));
// The C++11 fallback specifies noexcept; std::pair varies by toolchain.
#if 201402L > OMNI_CPLUSPLUS
  EXPECT_TRUE((std::is_nothrow_constructible<omni::compat::pair<int, int>,
    int, int>::value));
#endif
}

} // namespace

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
