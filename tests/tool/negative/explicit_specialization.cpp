// Expected failure: explicit template specializations are not supported
// reflection inputs.
#include <omnirefl/reflected_scope.hpp>

namespace negative_explicit_specialization {

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

} // namespace negative_explicit_specialization
