// Expected failure: constrained primary templates are not supported reflection
// inputs.
#include <omnirefl/reflected_scope.hpp>

namespace negative_constrained_primary_template {

template <typename T>
concept complete = 0 < sizeof(T);

template <complete T>
struct constrained_parameter {
  T value;
};

template <typename T>
  requires complete<T>
struct trailing_requires {
  T value;
};

void run() {
  (void)omni::reflected_call([](auto, auto) -> void {},
    omni::type<constrained_parameter<int>>,
    trailing_requires<int>{});
}

} // namespace negative_constrained_primary_template
