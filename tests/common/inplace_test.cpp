
#include "gtest_include.h"
#include "inplace_structs.h"

#include <omnirefl/reflected_call.hpp>
#include <omnirefl/reflected_scope.hpp>

#include <string>
#include <vector>

namespace example {
struct in_cpp_struct {
  std::string in_cpp_field_0;
  int in_cpp_field_1;
};

#if defined CXX_STANDARD && 11 < CXX_STANDARD
auto unnamed_returned_struct() {
  struct {
    int g_a = 4;
    double g_b = 8.15;
  } s{};
  return s;
}
#endif

} // namespace example

// todo: should be picked up as forward declaration
struct {
  std::string oceanic = "815";
} const unnamed_global{};

namespace example_impl {
struct print_field_names_simple_t {
  // c++11 friendly visitor
  struct _visit {
    template <typename... F>
    std::vector<std::string> operator()(const F &...field) {
      return {std::string(field.name())...};
    }
  };

  template <typename T>
  std::vector<std::string> operator()(const T &t) const {
    static_assert(omni::is_reflected<T>::value, "");
    const auto fields = omni::reflected(t).public_fields();
    return omni::compat::apply(_visit{}, fields);
  }
} const static print_field_names_simple{};
} // namespace example_impl

TEST(print_names, in_cpp_unnamed_global) {
  ASSERT_EQ((std::vector<std::string>{"oceanic"}),
    omni::reflected_call(example_impl::print_field_names_simple,
      unnamed_global));
}

TEST(print_names, in_header_struct) {
  const example::in_header_struct p{};
  const static std::vector<std::string> expected{
    "in_header_field_0",
    "in_header_field_1",
  };
  ASSERT_EQ(expected,
    omni::reflected_call(example_impl::print_field_names_simple, p));
}

#if defined CXX_STANDARD && 11 < CXX_STANDARD
TEST(print_names, in_cpp_unnamed_returned_struct) {
  ASSERT_EQ((std::vector<std::string>{"g_a", "g_b"}),
    omni::reflected_call(example_impl::print_field_names_simple,
      example::unnamed_returned_struct()));
}
#endif

TEST(print_names, in_cpp_struct) {
  const example::in_cpp_struct p{};
  const static std::vector<std::string> expected{
    "in_cpp_field_0",
    "in_cpp_field_1",
  };
  ASSERT_EQ(expected,
    omni::reflected_call(example_impl::print_field_names_simple, p));
}

// fixme: enable indexed types
TEST(print_names, in_cpp_local_unnamed_struct) {
  struct {
    std::string in_cpp_local_unnamed_field_0;
    int in_cpp_local_unnamed_field_1;
  } p{};

  ASSERT_EQ((std::vector<std::string>{
              "in_cpp_local_unnamed_field_0",
              "in_cpp_local_unnamed_field_1",
            }),
    omni::reflected_call(example_impl::print_field_names_simple, p));
}

TEST(print_names, in_cpp_local_unnamed_struct_with_base) {
  struct: example::in_cpp_struct {
    std::string in_cpp_local_unnamed_with_base_field_0;
    int in_cpp_local_unnamed_with_base_field_1;
  } p{};

  ASSERT_EQ((std::vector<std::string>{
              "in_cpp_field_0",
              "in_cpp_field_1",
              "in_cpp_local_unnamed_with_base_field_0",
              "in_cpp_local_unnamed_with_base_field_1",
            }),
    omni::reflected_call(example_impl::print_field_names_simple, p));
}

static const struct {
  template <typename Enum>
  std::vector<std::string> operator()(const Enum &) const noexcept {
    static_assert(omni::is_reflected<Enum>::value, "enum not reflected");
    const auto enums = omni::reflected_enum_t<Enum>::enumerators();
    std::vector<std::string> names;
    for (const auto &value_name : enums)
      names.emplace_back(value_name.second);

    return names;
  }
} get_enumerators{};

TEST(print_enums, in_cpp_local_unnamed_enum) {
  enum { unnamed_a, unnamed_b } const e{};

  static const std::vector<std::string> k_expected{"unnamed_a", "unnamed_b"};
  ASSERT_EQ(k_expected, omni::reflected_call(get_enumerators, e));

#if defined CXX_STANDARD && 11 < CXX_STANDARD
  ASSERT_EQ(k_expected,
    omni::reflected_call(
      // Inplace template lambdas are also possible (starting from C++14).
      // IMPORTANT NOTE: Trailing return type _must_ be specified, otherwise AST
      // parser will go inside the body to evaluate the return type, thus
      // breaking the tool run.
      [](const auto &v) -> std::vector<std::string> {
        using Enum = decltype(v);

        static_assert(omni::is_reflected<Enum>::value, "enum not reflected");
        const auto enums = omni::reflected_enum_t<Enum>::enumerators();
        std::vector<std::string> names;
        for (const auto &value_name : enums)
          names.emplace_back(value_name.second);

        return names;
      },
      e));

#endif
}
