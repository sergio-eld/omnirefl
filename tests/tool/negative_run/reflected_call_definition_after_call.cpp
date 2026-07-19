#include <omnirefl/reflection.hpp>

namespace negative_reflected_call_definition_after_call {

struct record;

void reflect(const record &value) {
  (void)omni::reflected_call([](auto) -> void {}, value);
}

// Keep the definition after reflect(): the type is incomplete at that call.
struct record {
  int value;
};

} // namespace negative_reflected_call_definition_after_call
