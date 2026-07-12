#include <omnirefl/reflection.hpp>

namespace negative_reflected_call_forward_declaration {

struct declared;

void run() {
  (void)omni::reflected_call(
    [](auto) -> void {},
    omni::type<declared>);
}

} // namespace negative_reflected_call_forward_declaration
