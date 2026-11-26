// Expected failure: template composition requests reflection before record's
// definition. A later valid instantiation must not hide the incomplete use.
#include <omnirefl/reflected_scope.hpp>

namespace composed_definition_after_use {

struct record;

template <typename T>
void reflect() {
  T value{};
  (void)omni::reflected_call([](auto) -> void {}, value);
}

template <typename T, int Instance>
void compose() {
  reflect<T>();
}

void before_definition() {
  compose<record, 0>();
}

struct record {
  int value;
};

void after_definition() {
  compose<record, 1>();
}

} // namespace composed_definition_after_use
