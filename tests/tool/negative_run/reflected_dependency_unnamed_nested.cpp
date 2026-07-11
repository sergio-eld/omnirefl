#include <omnirefl/reflection.hpp>

namespace negative_reflected_dependency_unnamed_nested {

struct record {
  enum {
    ready,
  } state;
};

void run() {
  record r{};

  (void)omni::reflected_call(
    [](auto) -> void {},
    r);
}

} // namespace negative_reflected_dependency_unnamed_nested
