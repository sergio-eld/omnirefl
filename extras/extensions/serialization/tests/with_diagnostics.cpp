#include <omnirefl/serialization/ryml/state.hpp>

#include <gtest/gtest.h>

#if defined(__GNUC__) && !defined(__clang__)
// GCC warns about GTest's internal conditionals in unbraced assertion guards.
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdangling-else"
#endif

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

struct double_value {
  int operator()(int value) const { return value * 2; }
};

TEST(with_diagnostics, transforms_values_in_each_reference_category) {
  omni::ryml::with_diagnostics_t<int, std::string> source{21, "warning"};
  const auto constant = source;

  const auto lvalue = source.transform(double_value{});
  const auto const_lvalue = constant.transform(double_value{});
  const auto rvalue = std::move(source).transform(double_value{});
  const auto const_rvalue = std::move(constant).transform(double_value{});

  EXPECT_EQ(42, lvalue.value);
  EXPECT_EQ("warning", lvalue.diagnostics);
  EXPECT_EQ(42, const_lvalue.value);
  EXPECT_EQ("warning", const_lvalue.diagnostics);
  EXPECT_EQ(42, rvalue.value);
  EXPECT_EQ("warning", rvalue.diagnostics);
  EXPECT_EQ(42, const_rvalue.value);
  EXPECT_EQ("warning", const_rvalue.diagnostics);
  EXPECT_EQ(21, constant.value);
  EXPECT_EQ("warning", constant.diagnostics);
}

TEST(with_diagnostics, transforms_an_empty_result_without_losing_errors) {
  const omni::ryml::with_diagnostics_t<int, std::string> source{
    omni::compat::nullopt, "invalid integer"};
  const auto result = source.transform(double_value{});

  EXPECT_FALSE(result.value);
  EXPECT_EQ("invalid integer", result.diagnostics);
}

struct read_owned_value {
  int operator()(std::unique_ptr<int> value) const { return *value; }
};

TEST(with_diagnostics,
  transforms_a_move_only_value_and_retains_its_owned_report) {
  omni::ryml::with_diagnostics_t<std::unique_ptr<int>,
    std::unique_ptr<std::string>> source{
    omni::compat::make_unique<int>(42),
    omni::compat::make_unique<std::string>("warning")};

  const auto result = std::move(source).transform(read_owned_value{});
  EXPECT_EQ(42, result.value);
  EXPECT_TRUE(result.diagnostics);
  if (result.diagnostics)
    EXPECT_EQ("warning", *result.diagnostics);
}

struct sequence_case {
  std::string name;
  omni::compat::optional<int> input;
  omni::compat::optional<long> expected;
  std::vector<std::string> reports;
};

class result_sequence: public testing::TestWithParam<sequence_case> {};

TEST_P(result_sequence, retains_reports_in_evaluation_order) {
  const auto &c = GetParam();
  const omni::ryml::with_diagnostics_t<int, std::vector<std::string>> source{
    c.input, {"first"}};

  const auto result = source.and_then([](int value)
    -> omni::ryml::with_diagnostics_t<long, std::vector<std::string>> {
    if (value < 0)
      return {omni::compat::nullopt, {"negative value"}};

    return {value * 2L, {"second"}};
  });

  EXPECT_EQ(c.expected, result.value);
  EXPECT_EQ(c.reports, result.diagnostics);
}

INSTANTIATE_TEST_SUITE_P(values,
  result_sequence,
  testing::Values(
    sequence_case{"available", 21, 42L, {"first", "second"}},
    sequence_case{"empty", {}, {}, {"first"}},
    sequence_case{"rejected", -1, {}, {"first", "negative value"}}),

  [](const testing::TestParamInfo<sequence_case> &c) { return c.param.name; });

struct increment_with_report {
  auto operator()(int value) const
    -> omni::ryml::with_diagnostics_t<long, std::string> {
    return {value + 1L, "second"};
  }
};

TEST(with_diagnostics, sequences_values_in_each_reference_category) {
  omni::ryml::with_diagnostics_t<int, std::string> source{41, "first"};
  const auto constant = source;

  const auto lvalue = source.and_then(increment_with_report{});
  const auto const_lvalue = constant.and_then(increment_with_report{});
  const auto rvalue = std::move(source).and_then(increment_with_report{});
  const auto const_rvalue =
    std::move(constant).and_then(increment_with_report{});

  EXPECT_EQ(42L, lvalue.value);
  EXPECT_EQ("first\nsecond", lvalue.diagnostics);
  EXPECT_EQ(42L, const_lvalue.value);
  EXPECT_EQ("first\nsecond", const_lvalue.diagnostics);
  EXPECT_EQ(42L, rvalue.value);
  EXPECT_EQ("first\nsecond", rvalue.diagnostics);
  EXPECT_EQ(42L, const_rvalue.value);
  EXPECT_EQ("first\nsecond", const_rvalue.diagnostics);
}

TEST(with_diagnostics, omits_empty_text_when_combining_reports) {
  const omni::ryml::with_diagnostics_t<int, std::string> empty{41, {}};
  const omni::ryml::with_diagnostics_t<int, std::string> reported{41, "first"};

  const auto first_empty = empty.and_then(increment_with_report{});
  const auto second_empty = reported.and_then([](int value)
    -> omni::ryml::with_diagnostics_t<int, std::string> {
    return {value + 1, {}};
  });

  EXPECT_EQ("second", first_empty.diagnostics);
  EXPECT_EQ("first", second_empty.diagnostics);
}

TEST(with_diagnostics, combines_issue_reports_and_keeps_the_first_policy) {
  omni::ryml::diagnostics first{};
  first.config.error_policy.error_limit = 3;
  omni::ryml::issue warning{};
  warning.reason = omni::json::issue_code::unknown_field;
  warning.warning = true;
  first.issues.push_back(warning);

  const omni::ryml::with_diagnostics_t<int> source{41, std::move(first)};
  const auto result = source.and_then([](int value)
    -> omni::ryml::with_diagnostics_t<int> {
    omni::ryml::diagnostics report{};
    omni::ryml::issue warning{};
    warning.reason = omni::json::issue_code::duplicate_field;
    warning.warning = true;
    report.issues.push_back(warning);
    return {value + 1, std::move(report)};
  });

  EXPECT_EQ(42, result.value);
  EXPECT_EQ(3, result.diagnostics.config.error_policy.error_limit);
  EXPECT_EQ(2U, result.diagnostics.issues.size());
  if (result.diagnostics.issues.size() == 2) {
    EXPECT_EQ(omni::json::issue_code::unknown_field,
      result.diagnostics.issues[0].reason);

    EXPECT_EQ(omni::json::issue_code::duplicate_field,
      result.diagnostics.issues[1].reason);
  }
}

struct custom_report { std::string text; };

TEST(with_diagnostics, uses_an_explicit_merger_for_custom_reports) {
  const omni::ryml::with_diagnostics_t<int, custom_report> source{
    41, {"first"}};
  const auto result = source.and_then([](int value)
    -> omni::ryml::with_diagnostics_t<int, custom_report> {
    return {value + 1, {"second"}};
  },

    [](custom_report first, custom_report second) -> custom_report {
      return {first.text + ":" + second.text};
    });

  EXPECT_EQ(42, result.value);
  EXPECT_EQ("first:second", result.diagnostics.text);
}

struct recover_value {
  auto operator()(std::string report) const
    -> omni::ryml::with_diagnostics_t<int, std::string> {
    return {42, report + ":recovered"};
  }
};

TEST(with_diagnostics, recovers_empty_values_in_each_reference_category) {
  omni::ryml::with_diagnostics_t<int, std::string> source{
    omni::compat::nullopt, "invalid"};
  const auto constant = source;

  const auto lvalue = source.or_else(recover_value{});
  const auto const_lvalue = constant.or_else(recover_value{});
  const auto rvalue = std::move(source).or_else(recover_value{});
  const auto const_rvalue = std::move(constant).or_else(recover_value{});

  EXPECT_EQ(42, lvalue.value);
  EXPECT_EQ("invalid:recovered", lvalue.diagnostics);
  EXPECT_EQ(42, const_lvalue.value);
  EXPECT_EQ("invalid:recovered", const_lvalue.diagnostics);
  EXPECT_EQ(42, rvalue.value);
  EXPECT_EQ("invalid:recovered", rvalue.diagnostics);
  EXPECT_EQ(42, const_rvalue.value);
  EXPECT_EQ("invalid:recovered", const_rvalue.diagnostics);
}

TEST(with_diagnostics, retains_available_values_instead_of_recovering) {
  const omni::ryml::with_diagnostics_t<int, std::string> source{7, "warning"};
  const auto result = source.or_else(recover_value{});

  EXPECT_EQ(7, result.value);
  EXPECT_EQ("warning", result.diagnostics);
}

struct report_length {
  std::size_t operator()(const std::string &report) const {
    return report.size();
  }
};

TEST(with_diagnostics, transforms_reports_in_each_reference_category) {
  omni::ryml::with_diagnostics_t<int, std::string> source{42, "warning"};
  const auto constant = source;

  const auto lvalue = source.transform_diagnostics(report_length{});
  const auto const_lvalue = constant.transform_diagnostics(report_length{});
  const auto rvalue = std::move(source).transform_diagnostics(report_length{});
  const auto const_rvalue =
    std::move(constant).transform_diagnostics(report_length{});

  EXPECT_EQ(42, lvalue.value);
  EXPECT_EQ(7U, lvalue.diagnostics);
  EXPECT_EQ(42, const_lvalue.value);
  EXPECT_EQ(7U, const_lvalue.diagnostics);
  EXPECT_EQ(42, rvalue.value);
  EXPECT_EQ(7U, rvalue.diagnostics);
  EXPECT_EQ(42, const_rvalue.value);
  EXPECT_EQ(7U, const_rvalue.diagnostics);
}

TEST(with_diagnostics, transforms_reports_without_recovering_a_value) {
  const omni::ryml::with_diagnostics_t<int, std::string> source{
    omni::compat::nullopt, "invalid"};
  const auto result = source.transform_diagnostics(report_length{});

  EXPECT_FALSE(result.value);
  EXPECT_EQ(7U, result.diagnostics);
}

TEST(with_diagnostics,
  sequences_owned_values_and_reports_with_a_custom_merger) {
  omni::ryml::with_diagnostics_t<std::unique_ptr<int>,
    std::unique_ptr<std::string>> source{
    omni::compat::make_unique<int>(41),
    omni::compat::make_unique<std::string>("first")};

  const auto result = std::move(source).and_then([](std::unique_ptr<int> value)
    -> omni::ryml::with_diagnostics_t<long,
      std::unique_ptr<std::string>> {
    return {*value + 1L, omni::compat::make_unique<std::string>("second")};
  },

    [](std::unique_ptr<std::string> first,
      std::unique_ptr<std::string> second) -> std::unique_ptr<std::string> {
      return omni::compat::make_unique<std::string>(*first + ":" + *second);
    });

  EXPECT_EQ(42L, result.value);
  EXPECT_TRUE(result.diagnostics);
  if (result.diagnostics)
    EXPECT_EQ("first:second", *result.diagnostics);
}

TEST(with_diagnostics, recovers_a_move_only_value_with_an_owned_report) {
  omni::ryml::with_diagnostics_t<std::unique_ptr<int>,
    std::unique_ptr<std::string>> source{
    omni::compat::nullopt,
    omni::compat::make_unique<std::string>("invalid")};

  const auto result = std::move(source).or_else(
    [](std::unique_ptr<std::string> report)
      -> omni::ryml::with_diagnostics_t<std::unique_ptr<int>,
        std::unique_ptr<std::string>> {
      return {omni::compat::make_unique<int>(42), std::move(report)};
    });

  EXPECT_TRUE(result.value);
  if (result.value)
    EXPECT_EQ(42, **result.value);

  EXPECT_TRUE(result.diagnostics);
  if (result.diagnostics)
    EXPECT_EQ("invalid", *result.diagnostics);
}

TEST(with_diagnostics,
  transforms_an_owned_report_and_moves_the_retained_value) {
  omni::ryml::with_diagnostics_t<std::unique_ptr<int>,
    std::unique_ptr<std::string>> source{
    omni::compat::make_unique<int>(42),
    omni::compat::make_unique<std::string>("warning")};

  const auto result = std::move(source).transform_diagnostics(
    [](std::unique_ptr<std::string> report) -> std::string {
      return std::move(*report);
    });

  EXPECT_TRUE(result.value);
  if (result.value)
    EXPECT_EQ(42, **result.value);

  EXPECT_EQ("warning", result.diagnostics);
}

struct add_owned_offset {
  std::unique_ptr<int> offset;

  int operator()(int value) && { return value + *this->offset; }
};

TEST(with_diagnostics, accepts_a_move_only_rvalue_callable) {
  const omni::ryml::with_diagnostics_t<int, std::string> source{1, "warning"};
  const auto result = source.transform(
    add_owned_offset{omni::compat::make_unique<int>(41)});

  EXPECT_EQ(42, result.value);
  EXPECT_EQ("warning", result.diagnostics);
}

} // namespace

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
