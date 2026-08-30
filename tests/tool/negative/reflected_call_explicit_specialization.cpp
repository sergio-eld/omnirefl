// Expected failure: explicit template specializations are not supported
// reflection inputs.
#include <omnirefl/reflection.hpp>

namespace negative_reflected_call_explicit_specialization {

template <typename T>
struct record {
  T value;
};

template <>
struct record<int> {
  int value;
};

void run() {
  record<int> value{};

  (void)omni::reflected_call([](auto) -> void {}, value);
}

} // namespace negative_reflected_call_explicit_specialization
