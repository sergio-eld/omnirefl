
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
} // namespace example

namespace example_impl {
struct print_field_names_simple_t {
  // c++11 friendly visitor
  struct collect_names {
    std::vector<std::string> &out;

    template <typename... F>
    void operator()(const F &...field) {
      out.reserve(sizeof...(field));
      const int _[]{(out.emplace_back(field.name()), 0)...};
      (void)_;
    }
  };

  template <typename T>
  void operator()(const T &t, std::vector<std::string> &out) const {
    static_assert(omni::is_reflected<T>::value, "");
    const auto fields = omni::reflected(t).fields();
    omni::compat::apply(collect_names{out}, fields);
  }
} const static print_field_names_simple{};
} // namespace example_impl

TEST(print_names, in_header_struct) {
  const example::in_header_struct p{};
  const static std::vector<std::string> expected{
    "in_header_field_0",
    "in_header_field_1",
  };
  std::vector<std::string> result;
  omni::reflected_call(example_impl::print_field_names_simple, p, result);
  ASSERT_EQ(expected, result);
}

TEST(print_names, in_cpp_struct) {
  const example::in_cpp_struct p{};
  const static std::vector<std::string> expected{
    "in_cpp_field_0",
    "in_cpp_field_1",
  };
  std::vector<std::string> result;
  omni::reflected_call(example_impl::print_field_names_simple, p, result);
  ASSERT_EQ(expected, result);
}

// fixme: enable indexed types
// TEST(print_names, in_cpp_local_unnamed_struct) {
//   struct {
//     std::string in_cpp_local_unnamed_field_0;
//     int in_cpp_local_unnamed_field_1;
//   } p{};
//   const static std::vector<std::string> expected{
//     "in_cpp_local_unnamed_field_0",
//     "in_cpp_local_unnamed_field_1",
//   };
//   std::vector<std::string> result;
//   omni::reflected_call(example_impl::print_field_names_simple, p, result);
//   ASSERT_EQ(expected, result);
// }
