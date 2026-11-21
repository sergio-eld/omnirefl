/** Named heterogeneous lookup: typed dispatch, owned errors, and expected-like
 * composition. C++11 cases also run in newer standards. */
#include <omnirefl/functional.hpp>

#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#if 201703L <= OMNI_CPLUSPLUS && defined(__has_include)
#  if __has_include(<string_view>)
#    include <string_view>
#  endif
#endif
#if 202002L < OMNI_CPLUSPLUS && defined(__has_include)
#  if __has_include(<expected>)
#    include <expected>
#  endif
#endif

namespace lookup_test {

struct account_id {
  static constexpr const char *name() { return "account_id"; }
  int value;
};

struct email {
  static constexpr const char *name() { return "email"; }
  int value;
};

struct empty_name {
  static constexpr const char *name() { return ""; }
  int value;
};

struct read_value {
  template <typename Meta>
  int operator()(const Meta &meta) const { return meta.value; }
};

struct read_type {
  std::string operator()(const account_id &) const { return "account"; }
  std::string operator()(const email &) const { return "email"; }
};

struct increment {
  int operator()(int value) const { return value + 1; }
};

struct describe_error {
  std::string operator()(
    const omni::fn::not_found_t<std::string> &error) const {
    return "unknown: " + error.name;
  }
};

template <typename Table>
struct alias_lookup {
  const Table &table;

  auto operator()(const omni::fn::not_found_t<std::string> &missing) const
    -> decltype(this->table.lookup(missing.name)) {
    return this->table.lookup(missing.name == "id" ? std::string{"account_id"}
      : missing.name);
  }
};

template <typename Table>
struct lookup_next {
  const Table &table;

  template <typename Meta>
  auto operator()(const Meta &) const
    -> decltype(this->table.lookup(std::string{})) {
    return this->table.lookup(std::string{"email"});
  }
};

struct reject_entry {
  template <typename Meta>
  auto operator()(const Meta &) const
    -> omni::fn::lookup_result_t<int, omni::fn::not_found_t<std::string>> {
    return omni::fn::lookup_result_t<int,
      omni::fn::not_found_t<std::string>>::failure(
        omni::fn::not_found_t<std::string>{"rejected"});
  }
};

struct make_owned_value {
  template <typename Meta>
  std::unique_ptr<int> operator()(const Meta &meta) const {
    return omni::compat::make_unique<int>(meta.value);
  }
};

struct consume_owned_value {
  auto operator()(std::unique_ptr<int> value) const
    -> omni::fn::lookup_result_t<int, omni::fn::not_found_t<std::string>> {
    return omni::fn::lookup_result_t<int,
      omni::fn::not_found_t<std::string>>::success(*value + 1);
  }
};

struct owned_error {
  auto operator()(omni::fn::not_found_t<std::string> error) const
    -> std::unique_ptr<std::string> {
    return omni::compat::make_unique<std::string>(std::move(error.name));
  }
};

struct ignore_value {
  template <typename Meta>
  void operator()(const Meta &) const {}
};

struct after_void {
  auto operator()() const
    -> omni::fn::lookup_result_t<int, omni::fn::not_found_t<std::string>> {
    return omni::fn::lookup_result_t<int,
      omni::fn::not_found_t<std::string>>::success(42);
  }
};

struct read_category {
  int operator()(int &value) const { return value + 1; }
  int operator()(const int &value) const { return value + 2; }
  int operator()(int &&value) const { return value + 3; }
  int operator()(const int &&value) const { return value + 4; }
};

template <typename Table, typename Key, typename = void>
struct accepts_key: std::false_type {};

template <typename Table, typename Key>
struct accepts_key<Table, Key, omni::compat::void_t<decltype(
  std::declval<const Table &>().lookup(std::declval<Key>()))>>:
    std::true_type {};

// Independent fixture text also needs constexpr indexing with C++11 libc++.
template <std::size_t Size>
struct name_text {
  char values[Size];

  constexpr char operator[](std::size_t index) const {
    return this->values[index];
  }
  constexpr std::size_t size() const { return Size; }
};

template <std::size_t Index>
struct numbered {
  static constexpr name_text<8> name() {
    return {{'f', 'i', 'e', 'l', 'd', '_',
      static_cast<char>('0' + Index / 10),
      static_cast<char>('0' + Index % 10)}};
  }
  int value;
};

template <std::size_t... Index>
constexpr auto numbered_table(omni::compat::index_sequence<Index...>)
  -> decltype(omni::fn::lookup_table(
    numbered<Index>{static_cast<int>(Index)}...)) {
  return omni::fn::lookup_table(numbered<Index>{static_cast<int>(Index)}...);
}

template <std::size_t Count>
auto threshold_results()
  -> std::array<omni::fn::lookup_result_t<int,
    omni::fn::not_found_t<std::string>>, 3> {
  constexpr auto table = numbered_table(
    omni::compat::make_index_sequence<Count>{});
  auto last = std::string{"field_00"};
  last[6] = static_cast<char>('0' + (Count - 1) / 10);
  last[7] = static_cast<char>('0' + (Count - 1) % 10);
  return {{
    table.lookup(std::string{"field_00"}).transform(read_value{}),
    table.lookup(last).transform(read_value{}),
    table.lookup(std::string{"field_XX"}).transform(read_value{}),
  }};
}

struct high_byte_name {
  static constexpr const char *name() { return "\xc3\xa9mail"; }
  int value;
};

template <std::size_t Length>
struct repeated_name {
  template <std::size_t... Index>
  static constexpr name_text<Length> text(
    omni::compat::index_sequence<Index...>) {
    return {{(static_cast<void>(Index), 'a')...}};
  }

  static constexpr name_text<Length> name() {
    return text(omni::compat::make_index_sequence<Length>{});
  }
};

template <std::size_t... Index>
constexpr auto varied_length_table(omni::compat::index_sequence<Index...>)
  -> decltype(omni::fn::lookup_table(repeated_name<Index + 1>{}...)) {
  return omni::fn::lookup_table(repeated_name<Index + 1>{}...);
}

template <std::size_t Index>
struct combination_name {
  static constexpr name_text<3> name() {
    return {{static_cast<char>('a' + Index / 9),
      static_cast<char>('a' + Index / 3 % 3),
      static_cast<char>('a' + Index % 3)}};
  }
  int value;
};

template <std::size_t... Index>
constexpr auto combination_table(omni::compat::index_sequence<Index...>)
  -> decltype(omni::fn::lookup_table(
    combination_name<Index>{static_cast<int>(Index)}...)) {
  return omni::fn::lookup_table(
    combination_name<Index>{static_cast<int>(Index)}...);
}

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
struct copy_error {};

struct rejects_copy {
  std::shared_ptr<int> resource;

  explicit rejects_copy(std::shared_ptr<int> value)
    : resource(std::move(value)) {}

  rejects_copy(const rejects_copy &other): resource(other.resource) {
    throw copy_error{};
  }

  rejects_copy(rejects_copy &&) = default;
};
#endif

} // namespace lookup_test

TEST(fn_lookup_table, constructs_a_constexpr_table_and_preserves_entry_values) {
  constexpr auto table = omni::fn::lookup_table(
    lookup_test::account_id{7}, lookup_test::email{9});

  const auto account = table.lookup(std::string{"account_id"})
    .transform(lookup_test::read_value{});
  const auto email = table.lookup(std::string{"email"})
    .transform(lookup_test::read_value{});

  EXPECT_TRUE(account.has_value());
  if (account.has_value()) {
    EXPECT_EQ(7, account.value());
  }
  EXPECT_TRUE(email.has_value());
  if (email.has_value()) {
    EXPECT_EQ(9, email.value());
  }
}

TEST(fn_lookup_table, passes_the_selected_concrete_type_to_the_visitor) {
  constexpr auto table = omni::fn::lookup_table(
    lookup_test::account_id{7}, lookup_test::email{9});
  const auto account = table.lookup(std::string{"account_id"})
    .transform(lookup_test::read_type{});
  const auto email = table.lookup(std::string{"email"})
    .transform(lookup_test::read_type{});

  EXPECT_EQ("account", account.value());
  EXPECT_EQ("email", email.value());
}

TEST(fn_lookup_table, owns_an_unmatched_key) {
  constexpr auto table = omni::fn::lookup_table(lookup_test::account_id{7});
  const auto result = table.lookup(std::string{"unknown"});

  EXPECT_FALSE(result.has_value());
  if (!result.has_value()) {
    EXPECT_EQ("unknown", result.error().name);
  }
}

TEST(fn_lookup_table, retains_an_error_through_value_transformation) {
  constexpr auto table = omni::fn::lookup_table(lookup_test::account_id{7});
  const auto result = table.lookup(std::string{"unknown"})
    .transform(lookup_test::read_value{})
    .transform(lookup_test::increment{});

  EXPECT_FALSE(result.has_value());
  if (!result.has_value()) {
    EXPECT_EQ("unknown", result.error().name);
  }
}

TEST(fn_lookup_table, supports_empty_names_and_exact_matching) {
  constexpr auto table = omni::fn::lookup_table(
    lookup_test::account_id{7}, lookup_test::empty_name{11});
  const auto empty = table.lookup(std::string{})
    .transform(lookup_test::read_value{});
  const auto prefix = table.lookup(std::string{"account"});
  const auto suffix = table.lookup(std::string{"account_ids"});

  EXPECT_TRUE(empty.has_value());
  if (empty.has_value()) {
    EXPECT_EQ(11, empty.value());
  }
  EXPECT_FALSE(prefix.has_value());
  if (!prefix.has_value()) {
    EXPECT_EQ("account", prefix.error().name);
  }
  EXPECT_FALSE(suffix.has_value());
  if (!suffix.has_value()) {
    EXPECT_EQ("account_ids", suffix.error().name);
  }
}

TEST(fn_lookup_table, recovers_an_alias_with_another_lookup) {
  constexpr auto table = omni::fn::lookup_table(
    lookup_test::account_id{7}, lookup_test::email{9});
  const auto result = table.lookup(std::string{"id"})
    .or_else(lookup_test::alias_lookup<decltype(table)>{table})
    .transform(lookup_test::read_value{});

  EXPECT_TRUE(result.has_value());
  if (result.has_value()) {
    EXPECT_EQ(7, result.value());
  }
}

TEST(fn_lookup_table, retains_a_found_value_during_error_recovery) {
  constexpr auto table = omni::fn::lookup_table(
    lookup_test::account_id{7}, lookup_test::email{9});
  const auto result = table.lookup(std::string{"email"})
    .or_else(lookup_test::alias_lookup<decltype(table)>{table})
    .transform(lookup_test::read_value{});

  EXPECT_TRUE(result.has_value());
  if (result.has_value()) {
    EXPECT_EQ(9, result.value());
  }
}

TEST(fn_lookup_table, changes_the_error_type) {
  constexpr auto table = omni::fn::lookup_table(lookup_test::account_id{7});
  const auto result = table.lookup(std::string{"unknown"})
    .transform_error(lookup_test::describe_error{});

  EXPECT_FALSE(result.has_value());
  if (!result.has_value()) {
    EXPECT_EQ("unknown: unknown", result.error());
  }
}

TEST(fn_lookup_table, retains_a_found_value_during_error_transformation) {
  constexpr auto table = omni::fn::lookup_table(lookup_test::account_id{7});
  const auto result = table.lookup(std::string{"account_id"})
    .transform_error(lookup_test::describe_error{})
    .transform(lookup_test::read_value{});

  EXPECT_TRUE(result.has_value());
  if (result.has_value()) {
    EXPECT_EQ(7, result.value());
  }
}

TEST(fn_lookup_table, binds_another_typed_lookup) {
  constexpr auto first = omni::fn::lookup_table(lookup_test::account_id{7});
  constexpr auto second = omni::fn::lookup_table(lookup_test::email{9});
  const auto result = first.lookup(std::string{"account_id"})
    .and_then(lookup_test::lookup_next<decltype(second)>{second})
    .transform(lookup_test::read_value{});

  EXPECT_TRUE(result.has_value());
  if (result.has_value()) {
    EXPECT_EQ(9, result.value());
  }
}

TEST(fn_lookup_table, retains_the_initial_error_through_a_bind) {
  constexpr auto table = omni::fn::lookup_table(lookup_test::account_id{7});
  const auto result = table.lookup(std::string{"unknown"})
    .and_then(lookup_test::reject_entry{});

  EXPECT_FALSE(result.has_value());
  if (!result.has_value()) {
    EXPECT_EQ("unknown", result.error().name);
  }
}

TEST(fn_lookup_table, propagates_the_bound_error) {
  constexpr auto table = omni::fn::lookup_table(lookup_test::account_id{7});
  const auto result = table.lookup(std::string{"account_id"})
    .and_then(lookup_test::reject_entry{});

  EXPECT_FALSE(result.has_value());
  if (!result.has_value()) {
    EXPECT_EQ("rejected", result.error().name);
  }
}

TEST(fn_lookup_table, moves_an_owned_transformed_value_into_a_bind) {
  constexpr auto table = omni::fn::lookup_table(lookup_test::account_id{7});
  auto owned = table.lookup(std::string{"account_id"})
    .transform(lookup_test::make_owned_value{});
  static_assert(!std::is_copy_constructible<decltype(owned)>::value,
    "A result with a move-only value is not copyable");
  const auto result = std::move(owned)
    .and_then(lookup_test::consume_owned_value{});

  EXPECT_TRUE(result.has_value());
  if (result.has_value()) {
    EXPECT_EQ(8, result.value());
  }
}

TEST(fn_lookup_table, supports_a_move_only_error) {
  constexpr auto table = omni::fn::lookup_table(lookup_test::account_id{7});
  auto owned = table.lookup(std::string{"unknown"})
    .transform_error(lookup_test::owned_error{});
  static_assert(!std::is_copy_constructible<decltype(owned)>::value,
    "A result with a move-only error is not copyable");
  const auto result = std::move(owned).transform(lookup_test::read_value{});

  EXPECT_FALSE(result.has_value());
  if (!result.has_value()) {
    EXPECT_TRUE(result.error());
    if (result.error()) {
      EXPECT_EQ("unknown", *result.error());
    }
  }
}

TEST(fn_lookup_table, preserves_the_value_category_for_transformations) {
  constexpr auto table = omni::fn::lookup_table(lookup_test::account_id{7});
  auto value = table.lookup(std::string{"account_id"})
    .transform(lookup_test::read_value{});
  const auto constant = table.lookup(std::string{"account_id"})
    .transform(lookup_test::read_value{});

  EXPECT_EQ(8, value.transform(lookup_test::read_category{}).value());
  EXPECT_EQ(9, constant.transform(lookup_test::read_category{}).value());
  EXPECT_EQ(10,
    std::move(value).transform(lookup_test::read_category{}).value());
  EXPECT_EQ(11,
    std::move(constant).transform(lookup_test::read_category{}).value());
}

TEST(fn_lookup_table, copies_and_moves_const_values) {
  const auto original = omni::fn::lookup_result_t<const int,
    std::string>::success(7);
  auto copied = original;
  const auto moved = std::move(copied);

  EXPECT_TRUE(moved.has_value());
  if (moved.has_value()) {
    EXPECT_EQ(7, moved.value());
  }
  EXPECT_EQ(7, original.value());
}

TEST(fn_lookup_table, copies_and_moves_const_errors) {
  const auto original = omni::fn::lookup_result_t<int,
    const std::string>::failure("missing");
  auto copied = original;
  const auto moved = std::move(copied);

  EXPECT_FALSE(moved.has_value());
  if (!moved.has_value()) {
    EXPECT_EQ("missing", moved.error());
  }
  EXPECT_EQ("missing", original.error());
}

TEST(fn_lookup_table, accepts_only_narrow_byte_key_expressions) {
  constexpr auto table = omni::fn::lookup_table(lookup_test::account_id{7});
  static_assert(lookup_test::accepts_key<decltype(table), std::string>::value,
    "Narrow strings are lookup keys");
  static_assert(lookup_test::accepts_key<decltype(table),
    const char (&)[11]>::value, "Narrow literals are lookup keys");
  static_assert(!lookup_test::accepts_key<decltype(table), int>::value,
    "Non-key arguments do not participate in lookup");
  static_assert(!lookup_test::accepts_key<decltype(table), std::wstring>::value,
    "Wide text is not a narrow byte key");
  static_assert(!lookup_test::accepts_key<decltype(table),
    std::array<int, 2>>::value, "Multi-byte ranges are not narrow byte keys");

  const auto found = table.lookup("account_id")
    .transform(lookup_test::read_value{});
  EXPECT_TRUE(found.has_value());
  if (found.has_value()) {
    EXPECT_EQ(7, found.value());
  }
}

TEST(fn_lookup_table, transforms_to_void_and_binds_without_arguments) {
  constexpr auto table = omni::fn::lookup_table(lookup_test::account_id{7});
  const auto result = table.lookup(std::string{"account_id"})
    .transform(lookup_test::ignore_value{})
    .and_then(lookup_test::after_void{});

  EXPECT_TRUE(result.has_value());
  if (result.has_value()) {
    EXPECT_EQ(42, result.value());
  }
}

#if defined(__cpp_lib_string_view) && 201606L <= __cpp_lib_string_view
namespace lookup_test {
struct view_name {
  static constexpr std::string_view name() { return {"account/alias", 7}; }
  int value;
};
} // namespace lookup_test

TEST(fn_lookup_table, accepts_view_keys_and_nonterminated_view_names) {
  constexpr auto table = omni::fn::lookup_table(lookup_test::view_name{7});
  const auto result = table.lookup(std::string_view{"account"})
    .transform(lookup_test::read_value{});

  EXPECT_TRUE(result.has_value());
  if (result.has_value()) {
    EXPECT_EQ(7, result.value());
  }
}
#endif

#if defined(__cpp_lib_expected) && 202211L <= __cpp_lib_expected
TEST(fn_lookup_table, binds_a_standard_expected_result) {
  constexpr auto table = omni::fn::lookup_table(lookup_test::account_id{7});
  const auto result = table.lookup(std::string{"account_id"})
    .and_then([](auto meta)
      -> std::expected<int, omni::fn::not_found_t<std::string>> {
      return meta.value + 1;
    });

  EXPECT_TRUE(result.has_value());
  if (result.has_value()) {
    EXPECT_EQ(8, result.value());
  }
}
#endif

TEST(fn_lookup_table, an_empty_table_retains_the_missing_key) {
  constexpr auto table = omni::fn::lookup_table();
  const auto result = table.lookup(std::string{"unknown"})
    .transform(lookup_test::read_value{})
    .and_then(lookup_test::reject_entry{})
    .transform_error(lookup_test::describe_error{});

  EXPECT_FALSE(result.has_value());
  if (!result.has_value()) {
    EXPECT_EQ("unknown: unknown", result.error());
  }
}

TEST(fn_lookup_table, recovers_from_an_empty_table) {
  constexpr auto empty = omni::fn::lookup_table();
  constexpr auto table = omni::fn::lookup_table(lookup_test::account_id{7});
  const auto result = empty.lookup(std::string{"id"})
    .or_else(lookup_test::alias_lookup<decltype(table)>{table})
    .transform(lookup_test::read_value{});

  EXPECT_TRUE(result.has_value());
  if (result.has_value()) {
    EXPECT_EQ(7, result.value());
  }
}

TEST(fn_lookup_table, constructs_a_constexpr_table_from_a_tuple) {
  constexpr auto original = omni::fn::lookup_table(
    lookup_test::account_id{7}, lookup_test::email{9});
  constexpr auto table = omni::fn::lookup_table(original.entries);
  const auto result = table.lookup("account_id")
    .transform(lookup_test::read_value{});

  EXPECT_TRUE(result.has_value());
  if (result.has_value()) {
    EXPECT_EQ(7, result.value());
  }
}

TEST(fn_lookup_table, searches_all_entries_in_a_nine_entry_mask_table) {
  const auto results = lookup_test::threshold_results<9>();
  EXPECT_TRUE(results[0].has_value());
  if (results[0].has_value()) {
    EXPECT_EQ(0, results[0].value());
  }
  EXPECT_TRUE(results[1].has_value());
  if (results[1].has_value()) {
    EXPECT_EQ(8, results[1].value());
  }
  EXPECT_FALSE(results[2].has_value());
  if (!results[2].has_value()) {
    EXPECT_EQ("field_XX", results[2].error().name);
  }
}

TEST(fn_lookup_table, searches_the_high_bit_of_a_sixty_four_entry_table) {
  const auto results = lookup_test::threshold_results<64>();
  EXPECT_TRUE(results[0].has_value());
  if (results[0].has_value()) {
    EXPECT_EQ(0, results[0].value());
  }
  EXPECT_TRUE(results[1].has_value());
  if (results[1].has_value()) {
    EXPECT_EQ(63, results[1].value());
  }
  EXPECT_FALSE(results[2].has_value());
  if (!results[2].has_value()) {
    EXPECT_EQ("field_XX", results[2].error().name);
  }
}

TEST(fn_lookup_table, searches_tables_larger_than_one_mask_word) {
  const auto results = lookup_test::threshold_results<65>();
  EXPECT_TRUE(results[0].has_value());
  if (results[0].has_value()) {
    EXPECT_EQ(0, results[0].value());
  }
  EXPECT_TRUE(results[1].has_value());
  if (results[1].has_value()) {
    EXPECT_EQ(64, results[1].value());
  }
  EXPECT_FALSE(results[2].has_value());
  if (!results[2].has_value()) {
    EXPECT_EQ("field_XX", results[2].error().name);
  }
}

TEST(fn_lookup_table, treats_mask_characters_as_unsigned_bytes) {
  constexpr auto table = omni::fn::lookup_table(lookup_test::numbered<0>{0},
    lookup_test::numbered<1>{1}, lookup_test::numbered<2>{2},
    lookup_test::numbered<3>{3}, lookup_test::numbered<4>{4},
    lookup_test::numbered<5>{5}, lookup_test::numbered<6>{6},
    lookup_test::numbered<7>{7}, lookup_test::high_byte_name{8});
  const auto found = table.lookup(std::string{"\xc3\xa9mail"})
    .transform(lookup_test::read_value{});
  const auto missing = table.lookup(std::string{"\xe3\xa9mail"});

  EXPECT_TRUE(found.has_value());
  if (found.has_value()) {
    EXPECT_EQ(8, found.value());
  }
  EXPECT_FALSE(missing.has_value());
}

TEST(fn_lookup_table, supports_many_names_with_increasing_lengths) {
  constexpr auto table = lookup_test::varied_length_table(
    omni::compat::make_index_sequence<32>{});
  const auto first = table.lookup(std::string(1, 'a'));
  const auto last = table.lookup(std::string(32, 'a'));
  const auto missing = table.lookup(std::string(33, 'a'));

  EXPECT_TRUE(first.has_value());
  EXPECT_TRUE(last.has_value());
  EXPECT_FALSE(missing.has_value());
  if (!missing.has_value()) {
    EXPECT_EQ(std::string(33, 'a'), missing.error().name);
  }
}

TEST(fn_lookup_table, distinguishes_three_independently_varying_characters) {
  constexpr auto table = lookup_test::combination_table(
    omni::compat::make_index_sequence<27>{});
  const auto first = table.lookup("aaa").transform(lookup_test::read_value{});
  const auto middle = table.lookup("abc").transform(lookup_test::read_value{});
  const auto last = table.lookup("ccc").transform(lookup_test::read_value{});
  const auto missing = table.lookup("abd");

  EXPECT_TRUE(first.has_value());
  if (first.has_value()) {
    EXPECT_EQ(0, first.value());
  }
  EXPECT_TRUE(middle.has_value());
  if (middle.has_value()) {
    EXPECT_EQ(5, middle.value());
  }
  EXPECT_TRUE(last.has_value());
  if (last.has_value()) {
    EXPECT_EQ(26, last.value());
  }
  EXPECT_FALSE(missing.has_value());
}

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
TEST(fn_lookup_table, a_throwing_value_copy_preserves_the_source) {
  auto resource = std::make_shared<int>(7);
  const std::weak_ptr<int> observer{resource};
  const auto source = omni::fn::lookup_result_t<lookup_test::rejects_copy,
    std::string>::success(std::move(resource));

  EXPECT_THROW(static_cast<void>(decltype(source){source}),
    lookup_test::copy_error);
  EXPECT_FALSE(observer.expired());
}

TEST(fn_lookup_table, a_throwing_error_copy_preserves_the_source) {
  auto resource = std::make_shared<int>(7);
  const std::weak_ptr<int> observer{resource};
  const auto source = omni::fn::lookup_result_t<int,
    lookup_test::rejects_copy>::failure(std::move(resource));

  EXPECT_THROW(static_cast<void>(decltype(source){source}),
    lookup_test::copy_error);
  EXPECT_FALSE(observer.expired());
}
#endif
