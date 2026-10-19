// Expected failure: the reflected type is incomplete at the call site even
// though its definition appears later in the translation unit.
#include <omnirefl/reflected_scope.hpp>

namespace negative_definition_after_use {

struct record;

void reflect(const record &value) {
  (void)omni::reflected_call([](auto) -> void {}, value);
}

// Keep the definition after reflect(): the type is incomplete at that call.
struct record {
  int value;
};

} // namespace negative_definition_after_use
