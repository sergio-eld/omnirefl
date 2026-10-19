// Expected failure: local and unnamed records require unsupported generated
// declarations when index mode is disabled.
#include <omnirefl/reflected_scope.hpp>

namespace negative_local_and_unnamed_types {

struct {
  int value;
} unnamed;

void run() {
  struct local {
    int value;
  };

  local first{};
  local second{};

  (void)omni::reflected_call([](auto...) -> void {}, first, second, unnamed);
}

} // namespace negative_local_and_unnamed_types
