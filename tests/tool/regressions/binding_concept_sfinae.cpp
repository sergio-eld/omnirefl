#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <type_traits>

namespace regression_binding_concept_sfinae {

struct record {
  int value;
};

struct visitor {
  template <typename Binding>
  auto operator()(Binding binding) const
    -> std::enable_if_t<omni::binding<Binding>, int> {
    return binding.value.value;
  }
};

} // namespace regression_binding_concept_sfinae

// FIXME(low): the out-of-scope query detector reports the expected tool-run
// binding_t<T&> instantiation when the public binding concept is evaluated in
// enable_if_t. A directly constrained template parameter works.
TEST(regression, DISABLED_binding_concept_is_valid_in_sfinae_return) {
  regression_binding_concept_sfinae::record input{17};
  EXPECT_EQ(17,
    omni::reflected_call(
      regression_binding_concept_sfinae::visitor{}, input));
}
