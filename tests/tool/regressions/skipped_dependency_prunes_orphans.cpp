#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

namespace regression_skipped_dependency_prunes_orphans {

struct orphan {
  int value;
};

template <typename T>
struct dependency {
  T value;
};

template <typename T>
struct dependency<T *> {
  orphan child;
};

struct record {
  dependency<int *> unsupported;
  int own_value;
};

} // namespace regression_skipped_dependency_prunes_orphans

TEST(regression, skipped_dependency_prunes_orphans) {
  regression_skipped_dependency_prunes_orphans::record value{};
  omni::reflected_call([](auto) -> void {}, value);
}
