#include "structs.h"

#include <gtest/gtest.h>

#include <mpark/variant.hpp>
#include <omnirefl/refl.hpp>

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// fixme: it seems that cmake doesn't pick up the changes that happen in an
// included header file. it makes sence, since only the .cpp files are tracked
// by cmake. There needs to be some solution, i.e.:
// - collect the headers via the tool and track them (too complex, roi low, not
// robust)
// - force the tool rerunning each time the .cpp file needs recompilation (is it
// even possible?)

// todo: special test for type alias reflected as a dependency
// todo: !!! diagnostics for failed reflection run due to compilation errors
namespace {

namespace test_util {

template <class...>
struct disjunction: std::false_type {};

template <class B1>
struct disjunction<B1>: B1 {};

template <class B1, class... Bn>
struct disjunction<B1, Bn...>:
    std::conditional<bool(B1::value), B1, disjunction<Bn...>>::type {};

template <template <typename...> class List, typename T, typename... Ts>
struct unique {
  using type = T;
};

template <template <typename...> class List,
  typename... Ts,
  typename U,
  typename... Us>
struct unique<List, List<Ts...>, U, Us...>:
    std::conditional<disjunction<std::is_same<U, Ts>...>::value,
      unique<List, List<Ts...>, Us...>,
      unique<List, List<Ts..., U>, Us...>>::type {};

template <typename... T>
using unique_variant =
  typename unique<mpark::variant, mpark::variant<>, T...>::type;

template <typename FunctionType>
struct return_type;

template <typename R, typename... T>
struct return_type<R(T...)> {
  using type = R;
};

template <typename>
struct variant_from_tuple;

template <typename... T>
struct variant_from_tuple<std::tuple<T...>> {
  using type = unique_variant<T...>;
};

template <typename Tuple>
using variant_element_t = typename variant_from_tuple<Tuple>::type;

struct {
  template <typename Tuple, size_t... I>
  static std::array<variant_element_t<Tuple>, sizeof...(I)> _impl(Tuple t,
    std::integer_sequence<std::size_t, I...>) {
    return {std::move(std::get<I>(t))...};
  }

  template <typename Tuple>
  auto operator()(Tuple tuple) const {
    return _impl(std::forward<Tuple>(tuple),
      std::make_integer_sequence<std::size_t, std::tuple_size<Tuple>{}>{});
  }
} const tuple_to_variant_array{};

} // namespace test_util

struct {
  // generic fallback:
  // let gtest handle anything that has operator== and PrintTo/<<
  template <typename T>
  void operator()(const T &lhs, const T &rhs) const {
    EXPECT_EQ(lhs, rhs);
  }

  void operator()(const example_impl::print_type_info_t::result &lhs,
    const example_impl::print_type_info_t::result &rhs) const {
    EXPECT_EQ(lhs.name, rhs.name);
    EXPECT_EQ(lhs.namespaces, rhs.namespaces);
  }

  void operator()(const example_impl::print_enum_type_info_t::result &lhs,
    const example_impl::print_enum_type_info_t::result &rhs) const {
    (*this)(lhs.type_info, rhs.type_info);
    EXPECT_EQ(lhs.names, rhs.names);
  }
} const gtest_cmp_eq{};

template <typename E, typename ReflectedImpl, typename T, typename... Args>
void test_reflected_call(const E &expected,
  const ReflectedImpl &impl,
  const T &t,
  Args &&...args) {
  gtest_cmp_eq(expected,
    omni::reflected_call(impl, t, std::forward<Args>(args)...));
}

template <typename Expected, typename T, typename... Args>
auto test_case(Expected e, T t, Args &&...args) {
  struct _test_case {
    Expected expected;
    T reflected_type;
    std::tuple<Args...> args;
  };

  return _test_case{
    std::move(e),
    std::move(t),
    std::make_tuple(std::forward<Args>(args)...),
  };
}

// todo: interface should allow:
// - specify COMMON for each test case inputs
// - specify a list of input paramters
// so that cases = COMMON * (test_case...) -> run_test(COMMON * test_case),
// run_test(COMMON * test_case), ...
//
// todo: expand test_case.args...
#define INSTANTIATE_REFLECTION_SUITE(SUITE_NAME, IMPL_OBJ, INPUTS) \
  struct SUITE_NAME: \
      ::testing::TestWithParam< \
        test_util::variant_element_t<decltype(INPUTS)>> {}; \
  TEST_P(SUITE_NAME, reflected_call) { \
    mpark::visit( \
      [](const auto &test_case) { \
        test_reflected_call(test_case.expected, \
          IMPL_OBJ, \
          test_case.reflected_type); \
      }, \
      GetParam()); \
  } \
  INSTANTIATE_TEST_SUITE_P(reflection_suite, \
    SUITE_NAME, \
    ::testing::ValuesIn(test_util::tuple_to_variant_array(INPUTS)))

TEST(print_names, simple) {
  {
    const example_types::championship v{};
    const static std::vector<std::string> expected{
      "name",
      "title",
    };
    const std::vector<std::string> result =
      omni::reflected_call(example_impl::print_field_names_simple, v);
    EXPECT_EQ(expected, result);
  }

  {
    const example_types::wrestler v{};
    const static std::vector<std::string> expected{
      "name",
      "age",
      "catchphrase",
      "titles",
      "info",
    };
    const std::vector<std::string> result =
      omni::reflected_call(example_impl::print_field_names_simple, v);
    EXPECT_EQ(expected, result);
  }
}

// todo: remove repetition, 'deduce' suite name from the reflection impl object
INSTANTIATE_REFLECTION_SUITE(print_field_names_recursive,
  example_impl::print_field_names_recursive,
  //> INPUTS
  std::make_tuple( //
    test_case(
      std::vector<std::string>{
        "name",
        "title",
      },
      example_types::championship{})

      ,
    test_case(
      std::vector<std::string>{
        "name",
        "age",
        "catchphrase",

        "titles",
        "titles[].name",
        "titles[].title",

        // fixme: add suport in header-mode
        "info",
        "info.ring_name",
        "info.signature_move",
        "info.debut_year",
      },
      example_types::wrestler{})

    // reflects member_sub (member field)
    ,
    test_case(
      std::vector<std::string>{
        "member",
        "member.ms_str",
        "member.ms_int",
      },
      example_types::with_member{})

    // reflects vec_elem (vector::value_type)
    ,
    test_case(
      std::vector<std::string>{
        "vec",
        "vec[].ve_str",
      },
      example_types::with_vec{})

    // reflects map_key (map::key_type)
    ,
    test_case(
      std::vector<std::string>{
        "mp",
      },
      example_types::with_map_key{})

    // reflects tuple_elem (std::tuple element)
    ,
    test_case(
      std::vector<std::string>{
        "tp",
      },
      example_types::with_tuple{})

    // fixme: generated code fails to compile
    // reflects variant_elem (std::variant alternative)
    // ,
    // test_case(
    //   std::vector<std::string>{
    //     "vr",
    //   },
    //   example_types::with_variant{})
    //< INPUTS
    ));

INSTANTIATE_REFLECTION_SUITE(print_enum_type_info_suite,
  example_impl::print_enum_type_info,
  //> INPUTS
  std::make_tuple(
    // namespace-scope unscoped enum
    test_case( //
      example_impl::print_enum_type_info_t::result{
        /*.type_info=*/{
          /*.name=*/"ring_style",
          /*.namespaces=*/{"example_types"},
        },
        /*.names =*/
        {
          "rs_technical",
          "rs_high_flying",
          "rs_power",
        },
      },
      example_types::ring_style{})

    // namespace-scope scoped enum
    ,
    test_case( //
      example_impl::print_enum_type_info_t::result{
        /*.type_info=*/{
          /*.name=*/"brand",
          /*.namespaces=*/{"example_types"},
        },
        /*.names =*/
        {
          "raw",
          "smackdown",
          "nxt",
        },
      },
      example_types::brand{})

    // dependency unscoped enum
    ,
    test_case( //
      example_impl::print_enum_type_info_t::result{
        /*.type_info=*/{
          /*.name=*/"title_rank",
          /*.namespaces=*/{"example_types", "dependency"},
        },
        /*.names =*/
        {
          "tr_midcard",
          "tr_main_event",
        },
      },
      example_types::dependency::title_rank{})

    // dependency scoped enum
    ,
    test_case( //
      example_impl::print_enum_type_info_t::result{
        /*.type_info=*/{
          /*.name=*/"promotion",
          /*.namespaces=*/{"example_types", "dependency"},
        },
        /*.names =*/
        {
          "wwe",
          "aew",
          "njpw",
        },
      },
      example_types::dependency::promotion{})
    //< INPUTS
    ));

// refactorme: use INSTANTIATE_REFLECTION_SUITE
TEST(print_values, recursive) {
  const example_types::wrestler v{
    "John Cena",
    47,
    "You can't see me",
    /*titles=*/
    {
      {"WWE Championship", "16-time champion"},
      {"World Heavyweight Championship", "3-time champion"},
      {"United States Championship", "5-time champion"},
      {"Royal Rumble", "2-time winner"},
      {"Money in the Bank", "1-time winner"},
      {"Tag Team Championship", "4-time champion"},
    },

    /*info=*/
    {
      "John Cena",
      "Attitude Adjustment",
      2002,
    },
  };

  const static std::vector<std::string> expected{
    // basic Fields
    "name: \"John Cena\"",
    "age: 47",
    "catchphrase: \"You can't see me\"",

    // titles (vector elements)
    "titles[0].name: \"WWE Championship\"",
    "titles[0].title: \"16-time champion\"",
    "titles[1].name: \"World Heavyweight Championship\"",
    "titles[1].title: \"3-time champion\"",
    "titles[2].name: \"United States Championship\"",
    "titles[2].title: \"5-time champion\"",
    "titles[3].name: \"Royal Rumble\"",
    "titles[3].title: \"2-time winner\"",
    "titles[4].name: \"Money in the Bank\"",
    "titles[4].title: \"1-time winner\"",
    "titles[5].name: \"Tag Team Championship\"",
    "titles[5].title: \"4-time champion\"",

    // nested info fields
    "info.ring_name: \"John Cena\"",
    "info.signature_move: \"Attitude Adjustment\"",
    "info.debut_year: 2002",
  };

  std::vector<std::string> result;
  omni::reflected_call(example_impl::print_field_values_recursive, v, result);
  EXPECT_EQ(expected, result);
}

TEST(modify_fields, simple) {
  const static std::map<std::string, std::string> input{
    {"str", "oceanic"},
    {"i", "815"},
  };
  const auto value = [](const std::string &k) { return input.find(k)->second; };
  example_types::settable output;
  omni::reflected_call(example_impl::simple_from_map, output, input);
  EXPECT_EQ(std::to_string(output.i), value("i"));
  EXPECT_EQ(output.str, value("str"));
}

} // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
