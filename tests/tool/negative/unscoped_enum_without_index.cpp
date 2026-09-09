// Expected failure: an unscoped enum cannot be forward-declared for generated
// reflection when index mode is disabled.
#include <omnirefl/reflected_scope.hpp>

namespace negative_unscoped_enum_without_index {

enum state {
  ready,
};

void run() {
  state value = ready;
  (void)omni::reflected_call([](auto) -> void {}, value);
}

} // namespace negative_unscoped_enum_without_index
