// Expected failure: local and unnamed records require unsupported generated
// declarations when index mode is disabled.
#include <omnirefl/reflection.hpp>

namespace negative_reflected_call_local_unnamed {

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

} // namespace negative_reflected_call_local_unnamed
