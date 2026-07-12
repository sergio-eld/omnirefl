#include <omnirefl/reflection.hpp>

namespace negative_reflected_call_unscoped_enum {

enum state {
  ready,
};

void run() {
  state value = ready;
  (void)omni::reflected_call([](auto) -> void {}, value);
}

} // namespace negative_reflected_call_unscoped_enum
