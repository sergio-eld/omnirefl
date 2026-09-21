// Expected warning: implicit assignment overloads make the pointer ambiguous.
#include <gtest/gtest.h>
#include <omnirefl/reflected_scope.hpp>

#include <tuple>

namespace diagnostic_skipped_implicit_overloaded_method {

struct record {
  int value;

  record &operator=(int replacement) {
    value = replacement;
    return *this;
  }
};

} // namespace diagnostic_skipped_implicit_overloaded_method

TEST(diagnostic, skipped_implicit_overloaded_method) {
  using diagnostic_skipped_implicit_overloaded_method::record;

  const std::size_t count = omni::reflected_call(
    [](auto binding) -> std::size_t {
      return std::tuple_size<decltype(binding.public_methods())>::value;
    },
    record{/*value=*/815});

  EXPECT_EQ(0u, count);
}
