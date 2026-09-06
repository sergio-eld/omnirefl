// Expected failure: partial template specializations are not supported
// reflection inputs.
#include <omnirefl/reflected_scope.hpp>

namespace negative_partial_specialization {

template <typename T>
struct record {
  T value;
};

template <typename T>
struct record<T *> {
  T *value;
};

void run() {
  int value = 0;
  record<int *> r{&value};

  (void)omni::reflected_call(
    [](auto) -> void {},
    r);
}

} // namespace negative_partial_specialization
