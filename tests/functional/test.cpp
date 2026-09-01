#include <omnirefl/functional.hpp>
#include <omnirefl/reflection.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace fn_test {

struct record {
  int count;
  std::string label;
  double ratio;
  int _cached_count;
};

struct without_internal_name {
  template <typename Field>
  constexpr bool operator()() const {
    return '_' != omni::compat::remove_cvref_t<Field>::name()[0];
  }
};

struct stringify_field {
  template <typename Field>
  std::string operator()(Field field) const {
    std::ostringstream value;
    value << field.value();
    return value.str();
  }
};

struct do_emplace_back {
  template <typename Container, typename Value>
  Container operator()(Container container, Value &&value) const {
    container.emplace_back(std::forward<Value>(value));
    return container;
  }
};

struct transform_record {
  template <typename T>
  // The tool pass needs the explicit result before generated metadata makes
  // `binding_t<T>::public_fields()` available for return-type deduction.
  std::vector<std::string> operator()(omni::binding_t<T> binding) const {
    return binding.public_fields() //
      | omni::fn::filter(without_internal_name{})
      | omni::fn::map(stringify_field{})
      | omni::fn::foldl(do_emplace_back{}, std::vector<std::string>{});
  }
};

struct increment {
  template <typename Value>
  void operator()(Value &value) const {
    ++value;
  }
};

struct consume {
  std::vector<std::unique_ptr<int>> &values;

  void operator()(std::unique_ptr<int> &&value) const {
    values.push_back(std::move(value));
  }
};

struct ignore {
  template <typename Value>
  void operator()(Value &&) const {}
};

struct observe {
  template <typename Value>
  constexpr Value operator()(Value value) const {
    return value;
  }
};

struct converted_value {
  int value;
};

template <typename>
struct another_type_tag {};

struct not_constructible {
  not_constructible(int) = delete;
};

template <typename T>
struct disabled_type_template {
  disabled_type_template(T) = delete;
};

#if defined(__cpp_deduction_guides) && 201703L <= __cpp_deduction_guides
template <typename T, std::size_t Size>
struct disabled_type_size_template {
  disabled_type_size_template(std::array<T, Size>) = delete;
};

template <typename T, std::size_t Size>
disabled_type_size_template(std::array<T, Size>)
  -> disabled_type_size_template<T, Size>;
#endif

template <template <typename> class Tag,
  typename To,
  typename Value,
  typename = void>
struct accepts_type_as: std::false_type {};

template <template <typename> class Tag, typename To, typename Value>
struct accepts_type_as<Tag,
  To,
  Value,
  omni::compat::void_t<decltype(omni::fn::as(Tag<To>{},
    std::declval<Value>()))>>: std::true_type {};

struct stringify {
  std::string operator()(int value) const {
    return std::to_string(value);
  }
};

struct mapped_values {
  int first;
  long second;
};

struct as_mapped_values {
  constexpr mapped_values operator()(int first, long second) const {
    return {first, second};
  }
};

struct transform {
  long operator()(int value) const {
    return value * 2L;
  }

  std::size_t operator()(const std::string &value) const {
    return value.size();
  }
};

struct quantity {
  double value;
};

struct to_double {
  double operator()(int value) const {
    return value;
  }

  double operator()(double value) const {
    return value;
  }

  double operator()(quantity value) const {
    return value.value;
  }
};

struct move_only {
  move_only() = default;
  move_only(const move_only &) = delete;
  move_only &operator=(const move_only &) = delete;
  move_only(move_only &&) = default;
  move_only &operator=(move_only &&) = delete;

  std::unique_ptr<int> operator()(std::unique_ptr<int> &&value) const {
    return std::move(value);
  }
};

struct subtract {
  template <typename Left, typename Right>
  constexpr auto operator()(Left &&left, Right &&right) const
    -> decltype(std::forward<Left>(left) - std::forward<Right>(right)) {
    return std::forward<Left>(left) - std::forward<Right>(right);
  }
};

struct add_owned {
  std::unique_ptr<int> operator()(std::unique_ptr<int> left,
    std::unique_ptr<int> right) const {
    return omni::compat::make_unique<int>(*left + *right);
  }
};

struct invoke_record {
  int value;

  constexpr invoke_record add(int right) const {
    return invoke_record{value + right};
  }

  constexpr int sum(int right) const {
    return value + right;
  }

  constexpr int noexcept_sum(int right) const noexcept {
    return value + right;
  }
};

constexpr invoke_record invoked_record{40};
static_assert(42
    == omni::compat::invoke(&invoke_record::sum, invoked_record, 2),
  "member function invocation must support constant evaluation");
static_assert(40 == omni::compat::invoke(&invoke_record::value, invoked_record),
  "member data invocation must support constant evaluation");
#if defined(__cpp_noexcept_function_type) \
  && 201510L <= __cpp_noexcept_function_type
static_assert(
  noexcept(
    omni::compat::invoke(&invoke_record::noexcept_sum, invoked_record, 2)),
  "member invocation must preserve noexcept");
static_assert(
  !noexcept(omni::compat::invoke(&invoke_record::sum, invoked_record, 2)),
  "member invocation must preserve potentially throwing calls");
#endif
static_assert(std::is_same<decltype(omni::compat::invoke(&invoke_record::value,
                             std::declval<invoke_record &>())),
                int &>::value,
  "member data invocation must preserve mutable lvalues");
static_assert(std::is_same<decltype(omni::compat::invoke(&invoke_record::value,
                             std::declval<const invoke_record &>())),
                const int &>::value,
  "member data invocation must preserve const lvalues");
static_assert(std::is_same<decltype(omni::compat::invoke(&invoke_record::value,
                             std::declval<invoke_record &&>())),
                int &&>::value,
  "member data invocation must preserve mutable rvalues");
static_assert(std::is_same<decltype(omni::compat::invoke(&invoke_record::value,
                             std::declval<const invoke_record &&>())),
                const int &&>::value,
  "member data invocation must preserve const rvalues");

struct derived_invoke_record: invoke_record {
  explicit constexpr derived_invoke_record(int value): invoke_record{value} {}
};

struct move_only_each {
  std::vector<int> &values;

  explicit move_only_each(std::vector<int> &values_): values{values_} {}
  move_only_each(const move_only_each &) = delete;
  move_only_each &operator=(const move_only_each &) = delete;
  move_only_each(move_only_each &&) = default;
  move_only_each &operator=(move_only_each &&) = delete;

  void operator()(int value) {
    values.push_back(value);
  }
};

struct add_next_owned_value {
  std::unique_ptr<int> value;

  void operator()(int &element) {
    element += (*value)++;
  }
};

struct add_owned_value {
  std::unique_ptr<int> value;

  int operator()(int element) const {
    return element + *value;
  }
};

struct move_only_fold {
  move_only_fold() = default;
  move_only_fold(const move_only_fold &) = delete;
  move_only_fold &operator=(const move_only_fold &) = delete;
  move_only_fold(move_only_fold &&) = default;
  move_only_fold &operator=(move_only_fold &&) = delete;

  int operator()(int left, int right) {
    return left + right;
  }
};

struct count_map_calls {
  std::size_t calls{};

  int operator()(int value) {
    ++calls;
    return value * 2;
  }
};

struct count_fold_calls {
  std::size_t calls{};

  int operator()(int left, int right) {
    ++calls;
    return left + right;
  }
};

struct count_unexpected_fold_calls {
  std::size_t &calls;

  template <typename Left, typename Right>
  int operator()(Left &&, Right &&) const {
    ++calls;
    return 0;
  }
};

enum class reference_category {
  mutable_lvalue,
  const_lvalue,
  mutable_rvalue,
  const_rvalue,
};

template <typename Reference>
struct reference_category_of:
    std::integral_constant<reference_category,
      std::is_lvalue_reference<Reference>::value
        ? (std::is_const<typename std::remove_reference<Reference>::type>::value
              ? reference_category::const_lvalue
              : reference_category::mutable_lvalue)
        : (std::is_const<typename std::remove_reference<Reference>::type>::value
              ? reference_category::const_rvalue
              : reference_category::mutable_rvalue)> {};

template <reference_category Category>
struct select_reference_category {
  template <typename Element>
  constexpr bool operator()() const {
    return Category == reference_category_of<Element>::value;
  }
};

struct select_integral {
  template <typename Element>
  constexpr bool operator()() const {
    return std::is_integral<omni::compat::remove_cvref_t<Element>>::value;
  }
};

struct select_unique_ptr {
  template <typename Element>
  constexpr bool operator()() const {
    return std::is_same<omni::compat::remove_cvref_t<Element>,
      std::unique_ptr<int>>::value;
  }
};

struct is_even {
  constexpr bool operator()(int value) const {
    return 0 == value % 2;
  }
};

struct equals {
  int expected;

  constexpr bool operator()(int value) const {
    return expected == value;
  }
};

struct move_only_equals {
  std::unique_ptr<int> expected;

  bool operator()(int value) const {
    return *expected == value;
  }
};

struct count_until {
  int expected;
  std::size_t &calls;

  bool operator()(int value) const {
    ++calls;
    return expected == value;
  }
};

template <typename T>
struct type_key {};

template <typename Left, typename Right>
constexpr bool operator==(type_key<Left>, type_key<Right>) {
  return std::is_same<Left, Right>::value;
}

struct element_type {
  template <typename Element>
  constexpr auto operator()() const
    -> type_key<omni::compat::remove_cvref_t<Element>> {
    return {};
  }
};

struct element_reference_category {
  template <typename Element>
  constexpr auto operator()() const
    -> type_key<std::integral_constant<reference_category,
      reference_category_of<Element>::value>> {
    return {};
  }
};

struct move_only_filter {
  move_only_filter() = default;
  move_only_filter(const move_only_filter &) = delete;
  move_only_filter &operator=(const move_only_filter &) = delete;
  move_only_filter(move_only_filter &&) = default;
  move_only_filter &operator=(move_only_filter &&) = delete;

  template <typename Element>
  constexpr bool operator()() const {
    return std::is_integral<omni::compat::remove_cvref_t<Element>>::value;
  }
};

template <typename Reference>
reference_category categorize() {
  using value_t = typename std::remove_reference<Reference>::type;
  if (std::is_lvalue_reference<Reference>::value)
    return std::is_const<value_t>::value //
      ? reference_category::const_lvalue
      : reference_category::mutable_lvalue;
  return std::is_const<value_t>::value //
    ? reference_category::const_rvalue
    : reference_category::mutable_rvalue;
}

struct collect_reference_category {
  std::vector<reference_category> &categories;

  template <typename Value>
  void operator()(Value &&) const {
    categories.push_back(categorize<Value &&>());
  }
};

struct map_reference_category {
  template <typename Value>
  reference_category operator()(Value &&) const {
    return categorize<Value &&>();
  }
};

struct reference_category_pair {
  reference_category left;
  reference_category right;
};

struct fold_reference_category {
  std::vector<reference_category_pair> &categories;

  template <typename Left, typename Right>
  int operator()(Left &&left, Right &&right) const {
    categories.push_back({categorize<Left &&>(), categorize<Right &&>()});
    return static_cast<int>(left) + static_cast<int>(right);
  }
};

struct count_visits {
  std::vector<int> &values;
  std::size_t calls;

  template <typename Value>
  void operator()(Value &&value) {
    values.push_back(static_cast<int>(value));
    ++calls;
  }
};

TEST(fn_composition, returns_stringified_reflected_field_values) {
  record input{3, "ignored", 2.5, 100};

  const auto values = omni::reflected_call(transform_record{}, input);

  EXPECT_EQ((std::vector<std::string>{"3", "ignored", "2.5"}), values);
}

TEST(compat_invoke, invokes_members_through_supported_object_forms) {
  invoke_record object{40};
  derived_invoke_record derived{40};
  auto *const pointer = &object;
  auto owned = omni::compat::make_unique<invoke_record>(invoke_record{40});
  auto reference = std::ref(object);

  EXPECT_EQ(42, omni::compat::invoke(&invoke_record::sum, object, 2));
  EXPECT_EQ(42, omni::compat::invoke(&invoke_record::sum, derived, 2));
  EXPECT_EQ(42, omni::compat::invoke(&invoke_record::sum, pointer, 2));
  EXPECT_EQ(42, omni::compat::invoke(&invoke_record::sum, owned, 2));
  EXPECT_EQ(42, omni::compat::invoke(&invoke_record::sum, reference, 2));
#if !defined(__cpp_lib_constexpr_functional) \
  || __cpp_lib_constexpr_functional < 201907L
  // The compatibility overload set must prioritize members of the wrapper
  // object over unwrapping it.
  EXPECT_EQ(&object,
    &omni::compat::invoke(&decltype(reference)::get, reference));
#endif

  omni::compat::invoke(&invoke_record::value, object) = 108;

  EXPECT_EQ(108, omni::compat::invoke(&invoke_record::value, pointer));
  EXPECT_EQ(108, omni::compat::invoke(&invoke_record::value, reference));
}

TEST(fn_as, converts_eager_lazy_and_piped_values) {
  const auto convert = omni::fn::as(stringify{});

  EXPECT_EQ("42", omni::fn::as(stringify{}, 42));
  EXPECT_EQ("108", convert(108));
  EXPECT_EQ("815", 815 | convert);
}

TEST(fn_as, preserves_the_converted_value_category) {
  int mutable_value{};
  const int const_value{};

  EXPECT_EQ(reference_category::mutable_lvalue,
    omni::fn::as(map_reference_category{}, mutable_value));
  EXPECT_EQ(reference_category::const_lvalue,
    omni::fn::as(map_reference_category{}, const_value));
  EXPECT_EQ(reference_category::mutable_rvalue,
    omni::fn::as(map_reference_category{}, std::move(mutable_value)));
  EXPECT_EQ(reference_category::const_rvalue,
    omni::fn::as(map_reference_category{}, std::move(const_value)));
}

TEST(fn_as, lazy_call_preserves_the_converted_value_category) {
  int mutable_value{};
  const int const_value{};
  const auto convert = omni::fn::as(map_reference_category{});

  EXPECT_EQ(reference_category::mutable_lvalue, convert(mutable_value));
  EXPECT_EQ(reference_category::const_lvalue, convert(const_value));
  EXPECT_EQ(reference_category::mutable_rvalue,
    convert(std::move(mutable_value)));
  EXPECT_EQ(reference_category::const_rvalue, convert(std::move(const_value)));
}

TEST(fn_as, pipe_preserves_the_converted_value_category) {
  int mutable_value{};
  const int const_value{};
  const auto convert = omni::fn::as(map_reference_category{});

  EXPECT_EQ(reference_category::mutable_lvalue, mutable_value | convert);
  EXPECT_EQ(reference_category::const_lvalue, const_value | convert);
  EXPECT_EQ(reference_category::mutable_rvalue,
    std::move(mutable_value) | convert);
  EXPECT_EQ(reference_category::const_rvalue, std::move(const_value) | convert);
}

TEST(fn_as, constructs_the_selected_type) {
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
  constexpr auto conversion = omni::type<converted_value>;
#else
  constexpr auto conversion = omni::type_t<converted_value>{};
#endif
  const auto construct = omni::fn::as(conversion);

  EXPECT_TRUE((
    std::is_same<converted_value, omni::type_t<converted_value>::type>::value));
  EXPECT_EQ(42, omni::fn::as(conversion, 42).value);
  EXPECT_EQ(108, construct(108).value);
  EXPECT_EQ(815, (815 | construct).value);
}

TEST(fn_as, stores_selected_type_construction) {
  const auto construct = omni::fn::as<converted_value>();

  EXPECT_EQ(42, construct(42).value);
  EXPECT_EQ(815, (815 | omni::fn::as<converted_value>()).value);
}

TEST(traits_is, compares_types_and_type_templates) {
  EXPECT_TRUE((omni::traits::is<int, int>()));
  EXPECT_FALSE((omni::traits::is<int, long>()));
  EXPECT_TRUE((omni::traits::is<omni::type_t, omni::type_t>()));
  EXPECT_FALSE((omni::traits::is<omni::type_t, another_type_tag>()));
  EXPECT_FALSE(
    (std::is_same<omni::type_t<int>, omni::compat::type_identity<int>>::value));
}

TEST(fn_as, participates_only_for_constructible_values) {
  EXPECT_TRUE((accepts_type_as<omni::type_t, converted_value, int>::value));
  EXPECT_FALSE((accepts_type_as<omni::type_t, not_constructible, int>::value));
  EXPECT_FALSE(
    (accepts_type_as<another_type_tag, converted_value, int>::value));
}

TEST(fn_as, constructs_a_class_template_from_the_value) {
  const auto convert = omni::fn::as(omni::fn::ctad<std::vector>());

  EXPECT_EQ((std::vector<int>{42}),
    omni::fn::as(omni::fn::ctad<std::vector>(), 42));
  EXPECT_EQ((std::vector<int>{108}), convert(108));
  EXPECT_EQ((std::vector<int>{815}), 815 | convert);
}

TEST(fn_as, stores_class_template_construction) {
  const auto construct = omni::fn::as<std::vector>();

  EXPECT_EQ((std::vector<int>{42}), construct(42));
  EXPECT_EQ((std::vector<int>{815}), 815 | omni::fn::as<std::vector>());
}

TEST(fn_ctad, constrains_the_returned_type_template_conversion) {
  EXPECT_FALSE(
    (omni::traits::is_type_template_constructible_from<disabled_type_template,
      int &&>::value));
  EXPECT_FALSE((omni::compat::is_invocable<
    decltype(omni::fn::ctad<disabled_type_template>()),
    int>::value));
}

#if defined(__cpp_deduction_guides) && 201703L <= __cpp_deduction_guides
TEST(fn_as, overloads_ctad_for_a_sized_class_template) {
  EXPECT_EQ((std::array<int, 1>{42}),
    42 | omni::fn::as(omni::fn::ctad<std::array>()));
}

TEST(fn_as, stores_sized_class_template_construction) {
  EXPECT_EQ((std::array<int, 1>{42}), 42 | omni::fn::as<std::array>());
}

TEST(fn_ctad, constrains_the_returned_type_size_template_conversion) {
  EXPECT_FALSE((omni::traits::is_type_size_template_constructible_from<
    disabled_type_size_template,
    std::array<int, 2> &&>::value));
  EXPECT_FALSE((omni::compat::is_invocable<
    decltype(omni::fn::ctad<disabled_type_size_template>()),
    std::array<int, 2>>::value));
}
#endif

TEST(fn_as, follows_the_available_class_template_deduction) {
  const std::vector<int> input{42};
  const auto result = input | omni::fn::as(omni::fn::ctad<std::vector>());

#if defined(__cpp_deduction_guides) && 201703L <= __cpp_deduction_guides
  EXPECT_TRUE((std::is_same<std::vector<int>,
    omni::compat::remove_cvref_t<decltype(result)>>::value));
  EXPECT_EQ(input, result);
#else
  EXPECT_TRUE((std::is_same<std::vector<std::vector<int>>,
    omni::compat::remove_cvref_t<decltype(result)>>::value));
  EXPECT_EQ((std::vector<std::vector<int>>{input}), result);
#endif
}

TEST(fn_as, forwards_a_move_only_value_into_the_selected_type) {
  auto source = omni::compat::make_unique<int>(42);

  const auto result =
    omni::fn::as(omni::type_t<std::unique_ptr<int>>{}, std::move(source));

  EXPECT_EQ(nullptr, source);
  EXPECT_NE(nullptr, result);
  if (result) {
    EXPECT_EQ(42, *result);
  }
}

TEST(fn_as, forwards_a_move_only_value) {
  auto source = omni::compat::make_unique<int>(42);

  const auto result = omni::fn::as(move_only{}, std::move(source));

  EXPECT_EQ(nullptr, source);
  EXPECT_NE(nullptr, result);
  if (result) {
    EXPECT_EQ(42, *result);
  }
}

TEST(fn_as, owns_a_move_only_lazy_conversion) {
  auto convert = omni::fn::as(add_owned_value{
    omni::compat::make_unique<int>(773),
  });

  EXPECT_EQ(815, 42 | std::move(convert));
}

TEST(fn_concat, concatenates_eager_lazy_and_piped_tuples) {
  const std::tuple<int, std::string> left{42, "value"};
  const std::pair<double, long> right{2.5, 815};
  const auto append = omni::fn::concat(right);

  const auto eager = omni::fn::concat(left, right);
  const auto called = append(left);
  const auto piped = left | append;

  EXPECT_EQ((std::tuple<int, std::string, double, long>{
              42,
              "value",
              2.5,
              815,
            }),
    eager);
  EXPECT_EQ(eager, called);
  EXPECT_EQ(eager, piped);
}

TEST(fn_concat, moves_elements_from_rvalue_tuples) {
  std::tuple<std::unique_ptr<int>> left{
    omni::compat::make_unique<int>(20),
  };
  std::tuple<std::unique_ptr<int>> right{
    omni::compat::make_unique<int>(22),
  };

  auto result = omni::fn::concat(std::move(left), std::move(right));
  std::unique_ptr<int> first;
  std::unique_ptr<int> second;
  std::tie(first, second) = std::move(result);
  const auto left_empty = omni::compat::apply(
    [](const std::unique_ptr<int> &value) { return !value; },
    left);
  const auto right_empty = omni::compat::apply(
    [](const std::unique_ptr<int> &value) { return !value; },
    right);

  ASSERT_NE(nullptr, first);
  EXPECT_EQ(20, *first);
  ASSERT_NE(nullptr, second);
  EXPECT_EQ(22, *second);
  EXPECT_TRUE(left_empty);
  EXPECT_TRUE(right_empty);
}

TEST(fn_concat, moves_an_owned_tuple_from_a_lazy_closure) {
  auto append =
    omni::fn::concat(std::make_tuple(omni::compat::make_unique<int>(22)));

  auto result = std::make_tuple(20) | std::move(append);
  int first{};
  std::unique_ptr<int> second;
  std::tie(first, second) = std::move(result);

  EXPECT_EQ(20, first);
  ASSERT_NE(nullptr, second);
  EXPECT_EQ(22, *second);
}

TEST(fn_any_of, evaluates_eager_lazy_and_piped_predicates) {
  const std::tuple<int, int, int> tuple{1, 3, 4};
  const auto equals_three = omni::fn::any_of(equals{3});
  std::size_t calls{};

  EXPECT_TRUE(omni::fn::any_of(is_even{}, tuple));
  EXPECT_TRUE(equals_three(tuple));
  EXPECT_TRUE(tuple | equals_three);
  EXPECT_TRUE(omni::fn::any_of(count_until{3, calls}, tuple));
  EXPECT_EQ(std::size_t{2}, calls);
  EXPECT_FALSE(omni::fn::any_of(is_even{}, std::tuple<>{}));
}

TEST(fn_any_of, moves_an_owned_predicate_from_a_lazy_closure) {
  auto contains =
    omni::fn::any_of(move_only_equals{omni::compat::make_unique<int>(22)});

  EXPECT_TRUE(std::move(contains)(std::make_tuple(20, 22)));
}

TEST(fn_none_of, evaluates_eager_lazy_and_piped_predicates) {
  const std::tuple<int, int, int> tuple{1, 3, 5};
  const auto even = omni::fn::none_of(is_even{});

  EXPECT_TRUE(omni::fn::none_of(is_even{}, tuple));
  EXPECT_TRUE(even(tuple));
  EXPECT_TRUE(tuple | even);
  EXPECT_TRUE(omni::fn::none_of(is_even{}, std::tuple<>{}));
}

TEST(fn_none_of, moves_an_owned_predicate_from_a_lazy_closure) {
  auto excludes =
    omni::fn::none_of(move_only_equals{omni::compat::make_unique<int>(42)});

  EXPECT_TRUE(std::make_tuple(20, 22) | std::move(excludes));
}

TEST(fn_not, negates_eager_lazy_and_type_predicates) {
  const auto not_three = omni::fn::not_(equals{3});
  const std::tuple<int, std::string, double> tuple{42, "value", 2.5};

  const auto non_integral =
    tuple | omni::fn::filter(omni::fn::not_(select_integral{}));

  EXPECT_TRUE(omni::fn::not_(is_even{}, 3));
  EXPECT_TRUE(not_three(4));
  EXPECT_TRUE(4 | not_three);
  EXPECT_EQ((std::tuple<std::string, double>{"value", 2.5}), non_integral);
}

TEST(fn_not, moves_an_owned_predicate_from_a_lazy_closure) {
  auto differs =
    omni::fn::not_(move_only_equals{omni::compat::make_unique<int>(22)});

  EXPECT_TRUE(std::move(differs)(20));
}

TEST(fn_diff_by, selects_unmatched_types_eager_lazy_and_piped) {
  const std::tuple<int, std::string, double, long> left{
    42,
    "value",
    2.5,
    815,
  };
  const std::tuple<std::string, long> right{"ignored", 108};
  const auto without_right_types = omni::fn::diff_by(element_type{}, right);

  const auto eager = omni::fn::diff_by(element_type{}, left, right);
  const auto called = without_right_types(left);
  const auto piped = left | without_right_types;

  EXPECT_EQ((std::tuple<int, double>{42, 2.5}), eager);
  EXPECT_EQ(eager, called);
  EXPECT_EQ(eager, piped);
}

TEST(fn_diff_by, moves_selected_values_only) {
  std::tuple<std::unique_ptr<int>, std::string> left{
    omni::compat::make_unique<int>(42),
    "retained",
  };
  const std::tuple<std::string> right{"matched"};

  auto result = std::move(left) | omni::fn::diff_by(element_type{}, right);
  std::unique_ptr<int> selected;
  std::tie(selected) = std::move(result);
  const auto source = omni::compat::apply(
    [](const std::unique_ptr<int> &moved, const std::string &retained) {
      return std::make_pair(!moved, retained);
    },
    left);

  ASSERT_NE(nullptr, selected);
  EXPECT_EQ(42, *selected);
  EXPECT_TRUE(source.first);
  EXPECT_EQ("retained", source.second);
}

TEST(fn_diff_by, moves_an_owned_right_tuple_from_a_lazy_closure) {
  auto without_owned = omni::fn::diff_by(element_type{},
    std::make_tuple(omni::compat::make_unique<int>(815)));

  const auto result = //
    std::make_tuple(omni::compat::make_unique<int>(42), 108) //
    | std::move(without_owned);

  EXPECT_EQ(std::make_tuple(108), result);
}

TEST(fn_diff_by, projects_forwarded_tuple_access_categories) {
  std::tuple<const int, int> lvalue{20, 22};
  std::tuple<int> lvalue_exclusion{0};
  std::tuple<const int, int> rvalue{20, 22};
  std::tuple<int> rvalue_exclusion{0};

  const auto from_lvalue =
    omni::fn::diff_by(element_reference_category{}, lvalue, lvalue_exclusion);
  const auto from_rvalue = omni::fn::diff_by(element_reference_category{},
    std::move(rvalue),
    std::move(rvalue_exclusion));

  EXPECT_EQ(std::make_tuple(20), from_lvalue);
  EXPECT_EQ(std::make_tuple(20), from_rvalue);
}

TEST(fn_filter, defers_call_and_pipe_application) {
  const std::tuple<int, std::string, double> tuple{1, "ignored", 2.5};
  const auto integral = omni::fn::filter(select_integral{});

  const auto called = integral(tuple);
  const auto piped = tuple | integral;

  EXPECT_EQ(std::make_tuple(1), called);
  EXPECT_EQ(std::make_tuple(1), piped);
}

TEST(fn_filter, preserves_tuple_access_categories) {
  std::tuple<int, double> mutable_lvalue_input{1, 1.5};
  const std::tuple<int, double> const_lvalue_input{2, 2.5};
  std::tuple<int, double> mutable_rvalue_input{3, 3.5};
  const std::tuple<int, double> const_rvalue_input{4, 4.5};

  const auto mutable_lvalue = omni::fn::filter(
    select_reference_category<reference_category::mutable_lvalue>{},
    mutable_lvalue_input);
  const auto const_lvalue = omni::fn::filter(
    select_reference_category<reference_category::const_lvalue>{},
    const_lvalue_input);
  const auto mutable_rvalue = omni::fn::filter(
    select_reference_category<reference_category::mutable_rvalue>{},
    std::move(mutable_rvalue_input));
  const auto const_rvalue = omni::fn::filter(
    select_reference_category<reference_category::const_rvalue>{},
    std::move(const_rvalue_input));

  EXPECT_EQ((std::tuple<int, double>{1, 1.5}), mutable_lvalue);
  EXPECT_EQ((std::tuple<int, double>{2, 2.5}), const_lvalue);
  EXPECT_EQ((std::tuple<int, double>{3, 3.5}), mutable_rvalue);
  EXPECT_EQ((std::tuple<int, double>{4, 4.5}), const_rvalue);
}

TEST(fn_filter, supports_standard_tuple_like_types) {
  const std::pair<int, std::string> pair{42, "ignored"};
  const std::array<int, 3> array{{1, 2, 3}};

  const auto filtered_pair = omni::fn::filter(select_integral{}, pair);
  const auto filtered_array = omni::fn::filter(select_integral{}, array);

  EXPECT_EQ(std::make_tuple(42), filtered_pair);
  EXPECT_EQ((std::tuple<int, int, int>{1, 2, 3}), filtered_array);
}

TEST(fn_filter, accepts_empty_and_fully_rejected_tuples) {
  const std::tuple<> empty;
  const std::tuple<std::string, double> rejected{"ignored", 2.5};

  const auto filtered_empty = omni::fn::filter(select_integral{}, empty);
  const auto filtered_rejected = omni::fn::filter(select_integral{}, rejected);

  EXPECT_TRUE(
    (std::is_same<omni::compat::remove_cvref_t<decltype(filtered_empty)>,
      std::tuple<>>::value));
  EXPECT_TRUE(
    (std::is_same<omni::compat::remove_cvref_t<decltype(filtered_rejected)>,
      std::tuple<>>::value));
}

TEST(fn_filter, moves_selected_values_from_an_rvalue_tuple) {
  std::tuple<std::unique_ptr<int>, std::string> tuple{
    omni::compat::make_unique<int>(42),
    "ignored",
  };

  auto filtered = omni::fn::filter(select_unique_ptr{}, std::move(tuple));
  std::unique_ptr<int> value;
  std::tie(value) = std::move(filtered);
  const auto source = omni::compat::apply(
    [](const std::unique_ptr<int> &moved, const std::string &retained) {
      return std::make_pair(!moved, retained);
    },
    tuple);

  EXPECT_EQ(42, value ? *value : 0);
  EXPECT_TRUE(source.first);
  EXPECT_EQ("ignored", source.second);
}

TEST(fn_filter, invokes_a_named_move_only_closure) {
  const std::tuple<int, std::string> tuple{42, "ignored"};
  auto integral = omni::fn::filter(move_only_filter{});

  const auto filtered = std::move(integral)(tuple);

  EXPECT_EQ(std::make_tuple(42), filtered);
}

#if OMNI_FN_HAS_PREDICATE_LAMBDA
TEST(fn_filter, adapts_a_templated_lambda_predicate) {
  const std::tuple<int, std::string, long> tuple{20, "ignored", 22};

  const auto filtered =
    tuple | omni::fn::filter(omni::fn::pred<[]<typename Element>() {
      return std::is_integral_v<std::remove_cvref_t<Element>>;
    }>);

  EXPECT_EQ((std::tuple<int, long>{20, 22}), filtered);
}
#endif

TEST(fn_each, visits_tuple_elements_in_order) {
  const std::tuple<int, int, int> tuple{1, 2, 3};
  std::vector<int> visited;

  omni::fn::each([&visited](int value) { visited.push_back(value); }, tuple);

  EXPECT_EQ((std::vector<int>{1, 2, 3}), visited);
}

TEST(fn_each, defers_call_and_pipe_application) {
  const std::tuple<int, int, int> tuple{1, 2, 3};
  std::vector<int> called;
  std::vector<int> piped;
  const auto call =
    omni::fn::each([&called](int value) { called.push_back(value); });

  call(tuple);
  tuple | omni::fn::each([&piped](int value) { piped.push_back(value); });

  EXPECT_EQ((std::vector<int>{1, 2, 3}), called);
  EXPECT_EQ((std::vector<int>{1, 2, 3}), piped);
}

TEST(fn_each, accepts_standard_tuple_like_types) {
  std::pair<int, long> pair{1, 2};
  std::array<int, 2> array{{3, 4}};

  omni::fn::each(increment{}, pair);
  omni::fn::each(increment{}, array);

  EXPECT_EQ((std::pair<int, long>{2, 3}), pair);
  EXPECT_EQ((std::array<int, 2>{{4, 5}}), array);
}

TEST(fn_each, invokes_member_function_pointers) {
  std::tuple<std::string, std::string> tuple{"first", "second"};

  omni::fn::each(&std::string::clear, tuple);

  EXPECT_EQ((std::tuple<std::string, std::string>{"", ""}), tuple);
}

TEST(fn_each, accepts_an_empty_tuple) {
  const std::tuple<> tuple;
  std::size_t visits{};

  omni::fn::each([&visits](int) { ++visits; }, tuple);

  EXPECT_EQ(std::size_t{0}, visits);
}

TEST(fn_each, preserves_rvalue_elements) {
  std::tuple<std::unique_ptr<int>> tuple{
    omni::compat::make_unique<int>(42),
  };
  std::vector<std::unique_ptr<int>> consumed;

  omni::fn::each(consume{consumed}, std::move(tuple));

  bool source_empty = false;
  omni::fn::each(
    [&source_empty](
      const std::unique_ptr<int> &value) { source_empty = !value; },
    tuple);

  EXPECT_TRUE(source_empty);
  EXPECT_EQ(std::size_t{1}, consumed.size());
  EXPECT_EQ(42, consumed.empty() || !consumed.front() ? 0 : *consumed.front());
}

TEST(fn_each, preserves_tuple_value_categories) {
  std::tuple<int> mutable_tuple{1};
  const std::tuple<int> const_tuple{2};
  std::vector<reference_category> categories;

  omni::fn::each(collect_reference_category{categories}, mutable_tuple);
  omni::fn::each(collect_reference_category{categories}, const_tuple);
  omni::fn::each(collect_reference_category{categories},
    std::move(mutable_tuple));
  omni::fn::each(collect_reference_category{categories},
    std::move(const_tuple));

  EXPECT_EQ((std::vector<reference_category>{
              reference_category::mutable_lvalue,
              reference_category::const_lvalue,
              reference_category::mutable_rvalue,
              reference_category::const_rvalue,
            }),
    categories);
}

TEST(fn_each, copies_an_lvalue_visitor) {
  const std::tuple<int, int> tuple{1, 2};
  std::vector<int> visited;
  count_visits visit{visited, 0};

  omni::fn::each(visit, tuple);

  EXPECT_EQ((std::vector<int>{1, 2}), visited);
  EXPECT_EQ(std::size_t{0}, visit.calls);
}

TEST(fn_each, moves_a_noncopyable_rvalue_visitor) {
  const std::tuple<int, int> tuple{1, 2};
  std::vector<int> visited;

  omni::fn::each(move_only_each{visited}, tuple);

  EXPECT_EQ((std::vector<int>{1, 2}), visited);
}

TEST(fn_each, invokes_and_pipes_a_noncopyable_visitor) {
  const std::tuple<int, int> tuple{1, 2};
  std::vector<int> called;
  std::vector<int> piped;
  auto call = omni::fn::each(move_only_each{called});

  std::move(call)(tuple);
  tuple | omni::fn::each(move_only_each{piped});

  EXPECT_EQ((std::vector<int>{1, 2}), called);
  EXPECT_EQ((std::vector<int>{1, 2}), piped);
}

TEST(fn_each, owns_mutable_state_in_a_lazy_visitor) {
  std::tuple<int, int, int> tuple{1, 2, 3};
  auto add =
    omni::fn::each(add_next_owned_value{omni::compat::make_unique<int>(10)});

  std::move(add)(tuple);

  EXPECT_EQ((std::tuple<int, int, int>{11, 13, 15}), tuple);
}

TEST(fn_map, transforms_elements_into_a_tuple) {
  const std::tuple<int, int, int> tuple{1, 2, 3};

  const auto mapped = omni::fn::map([](int value) { return value * 2; }, tuple);

  EXPECT_EQ((std::tuple<int, int, int>{2, 4, 6}), mapped);
}

TEST(fn_map, defers_call_and_pipe_application) {
  const std::tuple<int, int, int> tuple{1, 2, 3};
  const auto double_value = omni::fn::map([](int value) { return value * 2; });

  const auto called = double_value(tuple);
  const auto piped = tuple | double_value;

  EXPECT_EQ((std::tuple<int, int, int>{2, 4, 6}), called);
  EXPECT_EQ((std::tuple<int, int, int>{2, 4, 6}), piped);
}

TEST(fn_map, composes_with_fold_in_a_pipeline) {
  const std::tuple<int, double, quantity> tuple{1, 2.5, {3}};

  const auto result = tuple | omni::fn::map(to_double{})
    | omni::fn::foldl(
      [](double accumulated, double value) { return accumulated + value; },
      0.0);

  EXPECT_DOUBLE_EQ(6.5, result);
}

TEST(fn_map, transforms_heterogeneous_elements) {
  const std::tuple<int, std::string> tuple{3, "four"};

  auto mapped = omni::fn::map(transform{}, tuple);

  long doubled{};
  std::size_t length{};
  std::tie(doubled, length) = mapped;

  EXPECT_TRUE(
    (std::is_same<decltype(mapped), std::tuple<long, std::size_t>>::value));
  EXPECT_EQ(6, doubled);
  EXPECT_EQ(std::size_t{4}, length);
}

TEST(fn_map, supports_standard_tuple_like_types) {
  const std::pair<int, std::string> pair{3, "four"};
  const std::array<int, 2> array{{2, 4}};

  auto mapped_pair = omni::fn::map(transform{}, pair);
  auto mapped_array = omni::fn::map(transform{}, array);
  long pair_first{};
  std::size_t pair_second{};
  long array_first{};
  long array_second{};
  std::tie(pair_first, pair_second) = mapped_pair;
  std::tie(array_first, array_second) = mapped_array;

  EXPECT_EQ(6, pair_first);
  EXPECT_EQ(std::size_t{4}, pair_second);
  EXPECT_EQ(4, array_first);
  EXPECT_EQ(8, array_second);
}

TEST(fn_map, invokes_member_data_pointers) {
  const std::tuple<invoke_record, invoke_record> tuple{{1}, {2}};

  auto mapped = omni::fn::map(&invoke_record::value, tuple);
  int first{};
  int second{};
  std::tie(first, second) = mapped;

  EXPECT_EQ(1, first);
  EXPECT_EQ(2, second);
}

TEST(fn_map, maps_an_empty_tuple) {
  const std::tuple<> tuple;

  const auto mapped = omni::fn::map([](int value) { return value * 2; }, tuple);

  EXPECT_TRUE((std::is_same<omni::compat::remove_cvref_t<decltype(mapped)>,
    std::tuple<>>::value));
}

TEST(fn_map, preserves_tuple_value_categories) {
  std::tuple<int> mutable_tuple{1};
  const std::tuple<int> const_tuple{2};

  auto mutable_lvalue = omni::fn::map(map_reference_category{}, mutable_tuple);
  auto const_lvalue = omni::fn::map(map_reference_category{}, const_tuple);
  auto mutable_rvalue =
    omni::fn::map(map_reference_category{}, std::move(mutable_tuple));
  auto const_rvalue =
    omni::fn::map(map_reference_category{}, std::move(const_tuple));

  EXPECT_EQ(std::make_tuple(reference_category::mutable_lvalue),
    mutable_lvalue);
  EXPECT_EQ(std::make_tuple(reference_category::const_lvalue), const_lvalue);
  EXPECT_EQ(std::make_tuple(reference_category::mutable_rvalue),
    mutable_rvalue);
  EXPECT_EQ(std::make_tuple(reference_category::const_rvalue), const_rvalue);
}

TEST(fn_map, forwards_temporary_elements_as_rvalues) {
  auto mapped = std::tuple<int>{42} | omni::fn::map(map_reference_category{});
  reference_category category{};
  std::tie(category) = mapped;

  EXPECT_EQ(reference_category::mutable_rvalue, category);
}

TEST(fn_map, moves_from_rvalue_elements) {
  std::tuple<std::unique_ptr<int>> tuple{
    omni::compat::make_unique<int>(42),
  };

  auto mapped = omni::fn::map(move_only{}, std::move(tuple));
  std::unique_ptr<int> value;
  std::tie(value) = std::move(mapped);
  bool source_empty = false;
  omni::fn::each(
    [&source_empty](
      const std::unique_ptr<int> &source) { source_empty = !source; },
    tuple);

  EXPECT_TRUE(source_empty);
  EXPECT_EQ(42, value ? *value : 0);
}

TEST(fn_map, forwards_xvalue_elements_through_a_pipe) {
  std::tuple<std::unique_ptr<int>> tuple{
    omni::compat::make_unique<int>(42),
  };

  auto mapped = std::move(tuple) | omni::fn::map(move_only{});
  std::unique_ptr<int> value;
  std::tie(value) = std::move(mapped);

  EXPECT_EQ(42, value ? *value : 0);
}

TEST(fn_map, invokes_a_named_move_only_closure) {
  std::tuple<std::unique_ptr<int>> tuple{
    omni::compat::make_unique<int>(42),
  };
  auto transform_value = omni::fn::map(move_only{});

  auto mapped = std::move(transform_value)(std::move(tuple));
  std::unique_ptr<int> value;
  std::tie(value) = std::move(mapped);

  EXPECT_EQ(42, value ? *value : 0);
}

TEST(fn_map, owns_read_only_state_in_a_lazy_visitor) {
  const std::tuple<int, int, int> tuple{1, 2, 3};
  auto add = omni::fn::map(add_owned_value{omni::compat::make_unique<int>(10)});

  const auto mapped = std::move(add)(tuple);

  EXPECT_EQ((std::tuple<int, int, int>{11, 12, 13}), mapped);
}

TEST(fn_map, copies_an_lvalue_visitor) {
  const std::tuple<int, int> tuple{1, 2};
  count_map_calls visit;

  auto mapped = omni::fn::map(visit, tuple);
  int first{};
  int second{};
  std::tie(first, second) = mapped;

  EXPECT_EQ(2, first);
  EXPECT_EQ(4, second);
  EXPECT_EQ(std::size_t{0}, visit.calls);
}

TEST(fn_foldl, sums_tuple_elements) {
  const std::tuple<int, int, int> tuple{1, 2, 3};

  const auto result =
    omni::fn::foldl([](int sum, int value) { return sum + value; }, 0, tuple);

  EXPECT_EQ(6, result);
}

TEST(fn_foldl, defers_call_and_pipe_application) {
  const std::tuple<int, int, int> tuple{1, 2, 3};
  const auto sum = omni::fn::foldl(
    [](int accumulated, int value) { return accumulated + value; },
    0);

  const auto called = sum(tuple);
  const auto piped = tuple | sum;

  EXPECT_EQ(6, called);
  EXPECT_EQ(6, piped);
}

TEST(fn_foldl, applies_visit_left_to_right) {
  const std::tuple<short, long> tuple{short{3}, long{2}};

  const auto result = omni::fn::foldl(subtract{}, 10, tuple);

  EXPECT_EQ(5, result);
}

TEST(fn_foldl, supports_standard_tuple_like_types) {
  const std::pair<int, int> pair{10, 3};
  const std::array<int, 3> array{{10, 3, 2}};

  const auto pair_result = omni::fn::foldl(subtract{}, 20, pair);
  const auto array_result = omni::fn::foldl(subtract{}, 20, array);

  EXPECT_EQ(7, pair_result);
  EXPECT_EQ(5, array_result);
}

TEST(fn_foldl, invokes_member_function_pointers) {
  const std::tuple<int, int> tuple{3, 2};

  const auto result =
    omni::fn::foldl(&invoke_record::add, invoke_record{10}, tuple);

  EXPECT_EQ(15, result.value);
}

TEST(fn_foldl, preserves_tuple_value_categories) {
  std::tuple<int> mutable_tuple{1};
  const std::tuple<int> const_tuple{2};
  std::vector<reference_category_pair> categories;

  omni::fn::foldl(fold_reference_category{categories}, 0, mutable_tuple);
  omni::fn::foldl(fold_reference_category{categories}, 0, const_tuple);
  omni::fn::foldl(fold_reference_category{categories},
    0,
    std::move(mutable_tuple));
  omni::fn::foldl(fold_reference_category{categories},
    0,
    std::move(const_tuple));

  EXPECT_EQ(std::size_t{4}, categories.size());
  EXPECT_EQ(reference_category::mutable_rvalue, categories.at(0).left);
  EXPECT_EQ(reference_category::mutable_lvalue, categories.at(0).right);
  EXPECT_EQ(reference_category::mutable_rvalue, categories.at(1).left);
  EXPECT_EQ(reference_category::const_lvalue, categories.at(1).right);
  EXPECT_EQ(reference_category::mutable_rvalue, categories.at(2).left);
  EXPECT_EQ(reference_category::mutable_rvalue, categories.at(2).right);
  EXPECT_EQ(reference_category::mutable_rvalue, categories.at(3).left);
  EXPECT_EQ(reference_category::const_rvalue, categories.at(3).right);
}

TEST(fn_foldl, folds_move_only_values) {
  std::tuple<std::unique_ptr<int>, std::unique_ptr<int>, std::unique_ptr<int>>
    tuple{
      omni::compat::make_unique<int>(10),
      omni::compat::make_unique<int>(12),
      omni::compat::make_unique<int>(20),
    };

  auto result = omni::fn::foldl(add_owned{},
    omni::compat::make_unique<int>(0),
    std::move(tuple));
  std::vector<bool> source_empty;
  omni::fn::each(
    [&source_empty](
      const std::unique_ptr<int> &source) { source_empty.push_back(!source); },
    tuple);

  EXPECT_EQ((std::vector<bool>{true, true, true}), source_empty);
  EXPECT_EQ(42, result ? *result : 0);
}

TEST(fn_foldl, forwards_the_accumulator_as_an_rvalue) {
  std::tuple<int, int> tuple{2, 3};
  std::vector<reference_category_pair> categories;

  const auto result =
    omni::fn::foldl(fold_reference_category{categories}, 1, tuple);

  EXPECT_EQ(6, result);
  EXPECT_EQ(std::size_t{2}, categories.size());
  EXPECT_EQ(reference_category::mutable_rvalue, categories.at(0).left);
  EXPECT_EQ(reference_category::mutable_lvalue, categories.at(0).right);
  EXPECT_EQ(reference_category::mutable_rvalue, categories.at(1).left);
  EXPECT_EQ(reference_category::mutable_lvalue, categories.at(1).right);
}

TEST(fn_foldl, returns_the_accumulator_for_an_empty_tuple) {
  const std::tuple<> tuple;
  std::size_t calls{};

  auto result = omni::fn::foldl(count_unexpected_fold_calls{calls},
    omni::compat::make_unique<int>(42),
    tuple);

  EXPECT_EQ(42, result ? *result : 0);
  EXPECT_EQ(std::size_t{0}, calls);
}

TEST(fn_foldl, moves_a_lazy_accumulator) {
  const std::tuple<> tuple;
  std::size_t calls{};
  auto fold = omni::fn::foldl(count_unexpected_fold_calls{calls},
    omni::compat::make_unique<int>(42));

  auto result = tuple | std::move(fold);

  EXPECT_EQ(42, result ? *result : 0);
  EXPECT_EQ(std::size_t{0}, calls);
}

TEST(fn_foldl, folds_move_only_values_from_a_lazy_closure) {
  std::tuple<std::unique_ptr<int>, std::unique_ptr<int>> tuple{
    omni::compat::make_unique<int>(10),
    omni::compat::make_unique<int>(12),
  };
  auto fold = omni::fn::foldl(add_owned{}, omni::compat::make_unique<int>(20));

  auto result = std::move(fold)(std::move(tuple));

  EXPECT_EQ(42, result ? *result : 0);
}

TEST(fn_foldl, moves_a_noncopyable_lazy_visitor) {
  const std::tuple<int, int, int> tuple{10, 12, 20};
  auto fold = omni::fn::foldl(move_only_fold{}, 0);

  const auto result = std::move(fold)(tuple);

  EXPECT_EQ(42, result);
}

TEST(fn_foldl, copies_an_lvalue_visitor) {
  const std::tuple<int, int, int> tuple{1, 2, 3};
  count_fold_calls visit;

  const auto result = omni::fn::foldl(visit, 0, tuple);

  EXPECT_EQ(6, result);
  EXPECT_EQ(std::size_t{0}, visit.calls);
}

TEST(fn_foldr, reduces_from_right_to_left) {
  const std::tuple<std::string, std::string, std::string> tuple{
    "a",
    "b",
    "c",
  };

  const auto result = omni::fn::foldr(
    [](const std::string &value, std::string nested) {
      return value + '(' + nested + ')';
    },
    std::string{"end"},
    tuple);

  EXPECT_EQ("a(b(c(end)))", result);
}

TEST(fn_foldr, defers_call_and_pipe_application) {
  const std::tuple<int, int> tuple{10, 3};
  const auto subtract_values = omni::fn::foldr(subtract{}, 2);

  const auto called = subtract_values(tuple);
  const auto piped = tuple | subtract_values;

  EXPECT_EQ(9, called);
  EXPECT_EQ(9, piped);
}

TEST(fn_foldr, applies_visit_right_to_left) {
  const std::tuple<int, short> tuple{10, short{3}};

  const auto result = omni::fn::foldr(subtract{}, long{2}, tuple);

  EXPECT_EQ(9, result);
}

TEST(fn_foldr, supports_standard_tuple_like_types) {
  const std::pair<int, int> pair{10, 3};
  const std::array<int, 3> array{{10, 3, 2}};

  const auto pair_result = omni::fn::foldr(subtract{}, 0, pair);
  const auto array_result = omni::fn::foldr(subtract{}, 0, array);

  EXPECT_EQ(7, pair_result);
  EXPECT_EQ(9, array_result);
}

TEST(fn_foldr, invokes_member_function_pointers) {
  const std::tuple<invoke_record> tuple{{40}};

  const auto result = omni::fn::foldr(&invoke_record::sum, 2, tuple);

  EXPECT_EQ(42, result);
}

TEST(fn_foldr, preserves_tuple_value_categories) {
  std::tuple<int> mutable_tuple{1};
  const std::tuple<int> const_tuple{2};
  std::vector<reference_category_pair> categories;

  omni::fn::foldr(fold_reference_category{categories}, 0, mutable_tuple);
  omni::fn::foldr(fold_reference_category{categories}, 0, const_tuple);
  omni::fn::foldr(fold_reference_category{categories},
    0,
    std::move(mutable_tuple));
  omni::fn::foldr(fold_reference_category{categories},
    0,
    std::move(const_tuple));

  EXPECT_EQ(std::size_t{4}, categories.size());
  EXPECT_EQ(reference_category::mutable_lvalue, categories.at(0).left);
  EXPECT_EQ(reference_category::mutable_rvalue, categories.at(0).right);
  EXPECT_EQ(reference_category::const_lvalue, categories.at(1).left);
  EXPECT_EQ(reference_category::mutable_rvalue, categories.at(1).right);
  EXPECT_EQ(reference_category::mutable_rvalue, categories.at(2).left);
  EXPECT_EQ(reference_category::mutable_rvalue, categories.at(2).right);
  EXPECT_EQ(reference_category::const_rvalue, categories.at(3).left);
  EXPECT_EQ(reference_category::mutable_rvalue, categories.at(3).right);
}

TEST(fn_foldr, moves_a_stable_accumulator) {
  std::tuple<std::unique_ptr<int>, std::unique_ptr<int>, std::unique_ptr<int>>
    tuple{
      omni::compat::make_unique<int>(10),
      omni::compat::make_unique<int>(12),
      omni::compat::make_unique<int>(20),
    };

  auto result = omni::fn::foldr(add_owned{},
    omni::compat::make_unique<int>(0),
    std::move(tuple));
  std::vector<bool> source_empty;
  omni::fn::each(
    [&source_empty](
      const std::unique_ptr<int> &source) { source_empty.push_back(!source); },
    tuple);

  EXPECT_EQ((std::vector<bool>{true, true, true}), source_empty);
  EXPECT_EQ(42, result ? *result : 0);
}

TEST(fn_foldr, returns_the_accumulator_for_an_empty_tuple) {
  const std::tuple<> tuple;
  std::size_t calls{};

  auto result = omni::fn::foldr(count_unexpected_fold_calls{calls},
    omni::compat::make_unique<int>(42),
    tuple);

  EXPECT_EQ(42, result ? *result : 0);
  EXPECT_EQ(std::size_t{0}, calls);
}

TEST(fn_foldr, moves_a_lazy_accumulator) {
  const std::tuple<> tuple;
  std::size_t calls{};
  auto fold = omni::fn::foldr(count_unexpected_fold_calls{calls},
    omni::compat::make_unique<int>(42));

  auto result = tuple | std::move(fold);

  EXPECT_EQ(42, result ? *result : 0);
  EXPECT_EQ(std::size_t{0}, calls);
}

TEST(fn_foldr, folds_move_only_values_from_a_lazy_closure) {
  std::tuple<std::unique_ptr<int>, std::unique_ptr<int>> tuple{
    omni::compat::make_unique<int>(10),
    omni::compat::make_unique<int>(12),
  };
  auto fold = omni::fn::foldr(add_owned{}, omni::compat::make_unique<int>(20));

  auto result = std::move(fold)(std::move(tuple));

  EXPECT_EQ(42, result ? *result : 0);
}

TEST(fn_foldr, moves_a_noncopyable_lazy_visitor) {
  const std::tuple<int, int, int> tuple{10, 12, 20};
  auto fold = omni::fn::foldr(move_only_fold{}, 0);

  const auto result = std::move(fold)(tuple);

  EXPECT_EQ(42, result);
}

TEST(fn_foldr, copies_an_lvalue_visitor) {
  const std::tuple<int, int, int> tuple{1, 2, 3};
  count_fold_calls visit;

  const auto result = omni::fn::foldr(visit, 0, tuple);

  EXPECT_EQ(6, result);
  EXPECT_EQ(std::size_t{0}, visit.calls);
}

template <typename Tuple, typename = void>
struct accepts_each: std::false_type {};

template <typename Tuple>
struct accepts_each<Tuple,
  omni::compat::void_t<decltype(omni::fn::each(ignore{},
    std::declval<Tuple>()))>>: std::true_type {};

template <typename Tuple, typename = void>
struct accepts_map: std::false_type {};

template <typename Tuple>
struct accepts_map<Tuple,
  omni::compat::void_t<decltype(omni::fn::map(transform{},
    std::declval<Tuple>()))>>: std::true_type {};

template <typename Tuple, typename = void>
struct accepts_filter: std::false_type {};

template <typename Tuple>
struct accepts_filter<Tuple,
  omni::compat::void_t<decltype(omni::fn::filter(select_integral{},
    std::declval<Tuple>()))>>: std::true_type {};

template <typename Tuple, typename = void>
struct accepts_foldl: std::false_type {};

template <typename Tuple>
struct accepts_foldl<Tuple,
  omni::compat::void_t<
    decltype(omni::fn::foldl(subtract{}, 0, std::declval<Tuple>()))>>:
    std::true_type {};

template <typename Tuple, typename = void>
struct accepts_foldr: std::false_type {};

template <typename Tuple>
struct accepts_foldr<Tuple,
  omni::compat::void_t<
    decltype(omni::fn::foldr(subtract{}, 0, std::declval<Tuple>()))>>:
    std::true_type {};

template <typename Tuple, typename = void>
struct accepts_concat: std::false_type {};

template <typename Tuple>
struct accepts_concat<Tuple,
  omni::compat::void_t<decltype(omni::fn::concat(std::declval<Tuple>(),
    std::tuple<>{}))>>: std::true_type {};

template <typename Tuple, typename = void>
struct accepts_any_of: std::false_type {};

template <typename Tuple>
struct accepts_any_of<Tuple,
  omni::compat::void_t<decltype(omni::fn::any_of(is_even{},
    std::declval<Tuple>()))>>: std::true_type {};

template <typename Tuple, typename = void>
struct accepts_none_of: std::false_type {};

template <typename Tuple>
struct accepts_none_of<Tuple,
  omni::compat::void_t<decltype(omni::fn::none_of(is_even{},
    std::declval<Tuple>()))>>: std::true_type {};

template <typename Tuple, typename = void>
struct accepts_diff_by: std::false_type {};

template <typename Tuple>
struct accepts_diff_by<Tuple,
  omni::compat::void_t<decltype(omni::fn::diff_by(element_type{},
    std::declval<Tuple>(),
    std::tuple<>{}))>>: std::true_type {};

TEST(fn_contract, accepts_only_tuple_like_inputs) {
  EXPECT_TRUE((accepts_each<std::tuple<int> &>::value));
  EXPECT_TRUE((accepts_each<std::pair<int, int> &>::value));
  EXPECT_TRUE((accepts_each<std::array<int, 1> &>::value));
  EXPECT_FALSE((accepts_each<std::vector<int> &>::value));
  EXPECT_TRUE((accepts_map<std::tuple<int> &>::value));
  EXPECT_FALSE((accepts_map<std::vector<int> &>::value));
  EXPECT_TRUE((accepts_filter<std::tuple<int> &>::value));
  EXPECT_FALSE((accepts_filter<std::vector<int> &>::value));
  EXPECT_TRUE((accepts_foldl<std::tuple<int, int> &>::value));
  EXPECT_TRUE((accepts_foldl<std::tuple<> &>::value));
  EXPECT_FALSE((accepts_foldl<std::vector<int> &>::value));
  EXPECT_TRUE((accepts_foldr<std::tuple<int, int> &>::value));
  EXPECT_TRUE((accepts_foldr<std::tuple<> &>::value));
  EXPECT_FALSE((accepts_foldr<std::vector<int> &>::value));
  EXPECT_TRUE((accepts_concat<std::tuple<int> &>::value));
  EXPECT_FALSE((accepts_concat<std::vector<int> &>::value));
  EXPECT_TRUE((accepts_any_of<std::tuple<int> &>::value));
  EXPECT_FALSE((accepts_any_of<std::vector<int> &>::value));
  EXPECT_TRUE((accepts_none_of<std::tuple<int> &>::value));
  EXPECT_FALSE((accepts_none_of<std::vector<int> &>::value));
  EXPECT_TRUE((accepts_diff_by<std::tuple<int> &>::value));
  EXPECT_FALSE((accepts_diff_by<std::vector<int> &>::value));
}

TEST(fn_contract, exposes_lazy_closure_types) {
  EXPECT_TRUE((std::is_same<omni::fn::as_closure<observe>,
    decltype(omni::fn::as(observe{}))>::value));
  EXPECT_TRUE((std::is_same<omni::fn::each_closure<ignore>,
    decltype(omni::fn::each(ignore{}))>::value));
  EXPECT_TRUE((std::is_same<omni::fn::map_closure<transform>,
    decltype(omni::fn::map(transform{}))>::value));
  EXPECT_TRUE((std::is_same<omni::fn::filter_closure<select_integral>,
    decltype(omni::fn::filter(select_integral{}))>::value));
  EXPECT_TRUE((std::is_same<omni::fn::foldl_closure<subtract, int>,
    decltype(omni::fn::foldl(subtract{}, 0))>::value));
  EXPECT_TRUE((std::is_same<omni::fn::foldr_closure<subtract, int>,
    decltype(omni::fn::foldr(subtract{}, 0))>::value));
  EXPECT_TRUE((std::is_same<omni::fn::concat_closure<std::tuple<int>>,
    decltype(omni::fn::concat(std::tuple<int>{}))>::value));
  EXPECT_TRUE((std::is_same<omni::fn::any_of_closure<is_even>,
    decltype(omni::fn::any_of(is_even{}))>::value));
  EXPECT_TRUE((std::is_same<omni::fn::none_of_closure<is_even>,
    decltype(omni::fn::none_of(is_even{}))>::value));
  EXPECT_TRUE((std::is_same<omni::fn::not_closure<is_even>,
    decltype(omni::fn::not_(is_even{}))>::value));
  EXPECT_TRUE(
    (std::is_same<omni::fn::diff_by_closure<element_type, std::tuple<int>>,
      decltype(omni::fn::diff_by(element_type{}, std::tuple<int>{}))>::value));
}

static_assert(42 == omni::fn::as(observe{}, 42),
  "as must be usable in C++11 constexpr");

constexpr auto lazy_as = omni::fn::as(observe{});
static_assert(108 == lazy_as(108),
  "the lazy as call must be usable in C++11 constexpr");
static_assert(815 == (815 | omni::fn::as(observe{})),
  "the as pipe must be usable in C++11 constexpr");
static_assert(42 == omni::fn::as(omni::type_t<converted_value>{}, 42).value,
  "as must construct an explicitly selected type in C++11 constexpr");
constexpr auto lazy_selected_as = omni::fn::as<converted_value>();
static_assert(108 == lazy_selected_as(108).value,
  "the selected-type as closure must be usable in C++11 constexpr");

// The tool's bundled, standard-conforming C++11 libc++ does not provide
// constexpr tuple construction/access. These assertions contain no reflection
// queries; normal compiler builds still evaluate them in the C++11 matrix.
#if !defined(OMNI_TOOL_RUN) || 201402L <= OMNI_CPLUSPLUS
static_assert(
  (static_cast<void>(omni::fn::each(observe{}, std::tuple<int, int>{1, 2})),
    true),
  "each must be usable in C++11 constexpr");

constexpr auto lazy_each = omni::fn::each(observe{});
static_assert((static_cast<void>(lazy_each(std::tuple<int, int>{1, 2})), true),
  "the lazy each call must be usable in C++11 constexpr");
static_assert(
  (static_cast<void>(std::tuple<int, int>{1, 2} | omni::fn::each(observe{})),
    true),
  "the each pipe must be usable in C++11 constexpr");

constexpr auto map_result =
  omni::fn::map(observe{}, std::tuple<int, long>{1, 2});
constexpr auto map_values = omni::compat::apply(as_mapped_values{}, map_result);
static_assert(1 == map_values.first,
  "map must map the first element in a C++11 constant expression");
static_assert(2 == map_values.second,
  "map must map the second element in a C++11 constant expression");

constexpr auto lazy_map = omni::fn::map(observe{});
constexpr auto lazy_map_result = lazy_map(std::tuple<int, long>{1, 2});
constexpr auto lazy_map_values =
  omni::compat::apply(as_mapped_values{}, lazy_map_result);
static_assert(1 == lazy_map_values.first,
  "the lazy map call must map the first element in C++11 constexpr");
static_assert(2 == lazy_map_values.second,
  "the lazy map call must map the second element in C++11 constexpr");
constexpr auto piped_map_result =
  std::tuple<int, long>{1, 2} | omni::fn::map(observe{});
constexpr auto piped_map_values =
  omni::compat::apply(as_mapped_values{}, piped_map_result);
static_assert(1 == piped_map_values.first,
  "the map pipe must map the first element in C++11 constexpr");
static_assert(2 == piped_map_values.second,
  "the map pipe must map the second element in C++11 constexpr");

constexpr auto filter_result =
  omni::fn::filter(select_integral{}, std::tuple<int, double, long>{1, 2.5, 3});
static_assert(
  std::is_same<const std::tuple<int, long>, decltype(filter_result)>::value,
  "filter must produce the selected tuple type in C++11");
constexpr auto filter_values =
  omni::compat::apply(as_mapped_values{}, filter_result);
static_assert(1 == filter_values.first,
  "filter must preserve the first selected value in C++11");
static_assert(3 == filter_values.second,
  "filter must preserve the second selected value in C++11");

constexpr auto lazy_filter = omni::fn::filter(select_integral{});
constexpr auto lazy_filter_result =
  lazy_filter(std::tuple<int, double, long>{1, 2.5, 3});
constexpr auto lazy_filter_values =
  omni::compat::apply(as_mapped_values{}, lazy_filter_result);
static_assert(1 == lazy_filter_values.first,
  "the lazy filter call must preserve the first selected value in C++11");
static_assert(3 == lazy_filter_values.second,
  "the lazy filter call must preserve the second selected value in C++11");
constexpr auto piped_filter_result = std::tuple<int, double, long>{1, 2.5, 3}
  | omni::fn::filter(select_integral{});
constexpr auto piped_filter_values =
  omni::compat::apply(as_mapped_values{}, piped_filter_result);
static_assert(1 == piped_filter_values.first,
  "the filter pipe must preserve the first selected value in C++11");
static_assert(3 == piped_filter_values.second,
  "the filter pipe must preserve the second selected value in C++11");

static_assert(5 == omni::fn::foldl(subtract{}, 10, std::tuple<int, int>{3, 2}),
  "foldl must be C++11 constexpr");
static_assert(9 == omni::fn::foldr(subtract{}, 2, std::tuple<int, int>{10, 3}),
  "foldr must be C++11 constexpr");
static_assert(42 == omni::fn::foldl(subtract{}, 42, std::tuple<>{}),
  "foldl must return its accumulator for an empty tuple");
static_assert(42 == omni::fn::foldr(subtract{}, 42, std::tuple<>{}),
  "foldr must return its accumulator for an empty tuple");
static_assert(
  std::is_same<long,
    decltype(omni::fn::foldl(subtract{}, long{42}, std::tuple<>()))>::value,
  "the accumulator must provide the fold result type");

constexpr auto lazy_foldl = omni::fn::foldl(subtract{}, 10);
constexpr auto lazy_foldr = omni::fn::foldr(subtract{}, 2);
static_assert(5 == lazy_foldl(std::tuple<int, int>{3, 2}),
  "the lazy foldl call must be usable in C++11 constexpr");
static_assert(9 == lazy_foldr(std::tuple<int, int>{10, 3}),
  "the lazy foldr call must be usable in C++11 constexpr");
static_assert(5
    == (std::tuple<int, int>{3, 2} | omni::fn::foldl(subtract{}, 10)),
  "the foldl pipe must be usable in C++11 constexpr");
static_assert(9
    == (std::tuple<int, int>{10, 3} | omni::fn::foldr(subtract{}, 2)),
  "the foldr pipe must be usable in C++11 constexpr");
#endif

} // namespace fn_test
