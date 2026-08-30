// Expected failure: partial template specializations are not supported
// reflection inputs.
#include <omnirefl/reflection.hpp>

namespace negative_reflected_call_partial_specialization {

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

} // namespace negative_reflected_call_partial_specialization
