#include <gtest/gtest.h>
#include <omnirefl/reflected_scope.hpp>

#include <tuple>

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

struct dependency_queries {
  bool parameter_is_reflected;
  bool return_type_is_reflected;
};

struct inspect_function {
  template <typename Function>
  dependency_queries operator()(Function) const {
    using parameter = typename std::tuple_element</*parameter=*/0,
      typename Function::parameters_t>::type;

    return {
      /*parameter_is_reflected=*/
      omni::is_reflected<typename parameter::type>::value,
      /*return_type_is_reflected=*/
      omni::is_reflected<typename Function::return_type>::value,
    };
  }
};

struct inspect {
  template <typename _M>
  dependency_queries operator()(omni::record_meta_t<_M> meta) const {
    return omni::compat::apply(inspect_function{},
      meta.accessible_member_functions());
  }
};

} // namespace diagnostic_local_return

TEST(diagnostic, local_member_return_preserves_parameter_dependencies) {
  namespace example = diagnostic_local_return;

  const auto reflected = omni::reflected_call(example::inspect{},
    omni::type_t<example::record>{});

  EXPECT_TRUE(reflected.parameter_is_reflected);
  EXPECT_FALSE(reflected.return_type_is_reflected);
}
