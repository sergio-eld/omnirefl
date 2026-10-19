// Expected failure: forward declarations without definitions cannot be
// reflected.
#include <omnirefl/reflected_scope.hpp>

namespace negative_incomplete_types {

struct declared;

template <typename T>
struct incomplete_template;

void reflect_binding(const declared &value) {
  (void)omni::reflected_call([](auto) -> void {}, value);
}

void run() {
  (void)omni::reflected_call([](auto) -> void {}, omni::type<declared>);

  (void)omni::reflected_call([](auto) -> void {},
    omni::type<incomplete_template<int>>);
}

} // namespace negative_incomplete_types
