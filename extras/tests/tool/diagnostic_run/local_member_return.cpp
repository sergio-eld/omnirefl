#include <gtest/gtest.h>
#include <omnirefl/reflected_scope.hpp>

#include <tuple>
#include <utility>

namespace diagnostic_local_return {

struct payload {
  int value;
};

struct record {
  auto make(payload input) {
    struct result {
      int value;
    };

    return result{input.value};
  }
};

struct inspect_function {
  template <typename Function>
  std::pair<bool, bool> operator()(Function) const {
    using parameter = typename std::tuple_element</*parameter=*/0,
      typename Function::parameters_t>::type;

    return {
      omni::is_reflected<typename parameter::type>::value,
      omni::is_reflected<typename Function::return_type>::value,
    };
  }
};

struct inspect {
  template <typename _M>
  std::pair<bool, bool> operator()(omni::record_meta_t<_M> meta) const {
    return omni::compat::apply(inspect_function{},
      meta.accessible_member_functions());
  }
};

} // namespace diagnostic_local_return

TEST(diagnostic, local_member_return_preserves_parameter_dependencies) {
  namespace example = diagnostic_local_return;

  const auto reflected = omni::reflected_call(example::inspect{},
    omni::type_t<example::record>{});

  EXPECT_TRUE(reflected.first); // Parameter record receives metadata.
  EXPECT_FALSE(reflected.second); // Function-local return record does not.
}
