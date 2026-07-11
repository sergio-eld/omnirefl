#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

namespace diagnostic_reflected_dependency_unnamed_nested {

struct record {
  enum {
    ready,
  } state;
};

} // namespace diagnostic_reflected_dependency_unnamed_nested

TEST(diagnostic, reflected_dependency_unnamed_nested) {
  diagnostic_reflected_dependency_unnamed_nested::record value{};
  omni::reflected_call([](auto) -> void {}, value);
}
