// Expected failure: an unscoped enum cannot be forward-declared for generated
// reflection without a fixed underlying type.
#include <omnirefl/reflected_scope.hpp>

namespace negative_unfixed_enum {

enum state {
  ready,
};

void run() {
  state value = ready;
  (void)omni::reflected_call([](auto) -> void {}, value);
}

} // namespace negative_unfixed_enum
