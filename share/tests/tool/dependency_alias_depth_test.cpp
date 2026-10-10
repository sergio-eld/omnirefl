// A finite chain of distinct alias dependencies must reach and reflect its
// leaf without being mistaken for an unbounded chain.
#include <gtest/gtest.h>
#include <omnirefl/reflected_scope.hpp>

#include <cstddef>
#include <type_traits>
#include <vector>

namespace dependency_alias_depth_test {

struct leaf {
  int value;
};

template <std::size_t Depth, typename T>
struct grow {
  using value_type = typename std::conditional<0 == Depth,
    T,
    grow<Depth - 1, std::vector<T>>>::type;
};

struct record {
  using value_type = grow<10, leaf>;
  // Not part of the dependency protocol; keeps the assertion dependent until
  // the callable body becomes a reflected scope.
  using expected_dependency = leaf;
};

struct verify_dependency {
  template <typename Binding>
  void operator()(Binding) const {
    using dependency = typename Binding::type::expected_dependency;
    EXPECT_TRUE(omni::is_reflected<dependency>::value);
  }
};

} // namespace dependency_alias_depth_test

TEST(dependency_alias, resolves_finite_chain) {
  dependency_alias_depth_test::record value{};
  omni::reflected_call(dependency_alias_depth_test::verify_dependency{}, value);
}
