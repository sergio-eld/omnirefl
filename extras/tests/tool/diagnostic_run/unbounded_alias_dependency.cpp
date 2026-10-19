// Disabled detection case: each alias produces a distinct specialization:
//   grow<int> -> grow<std::vector<int>>
//             -> grow<std::vector<std::vector<int>>> -> ...
// Declaration identity therefore cannot close the traversal.
#include <gtest/gtest.h>
#include <omnirefl/reflected_scope.hpp>

#include <vector>

namespace diagnostic_unbounded_alias_dependency {

template <typename T>
struct grow {
  using value_type = grow<std::vector<T>>;
};

struct record {
  using value_type = grow<int>;
};

} // namespace diagnostic_unbounded_alias_dependency

TEST(diagnostic, unbounded_alias_dependency) {
  diagnostic_unbounded_alias_dependency::record value{};
  omni::reflected_call([](auto) -> void {}, value);
}
