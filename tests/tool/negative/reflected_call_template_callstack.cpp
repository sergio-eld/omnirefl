// Expected failure: reflection of an incomplete type is diagnosed through the
// instantiated template call stack.
#include <omnirefl/reflection.hpp>

namespace negative_reflected_call_template_callstack {

struct record {
  int value;
};

struct declared;

template <typename T>
void reflect_from_template(T &value) {
  (void)omni::reflected_call([](auto, auto) -> void {},
    value,
    omni::type<declared>);
}

template <typename T>
void instantiate_reflection(T &value) {
  reflect_from_template(value);
}

void run() {
  record value{};
  instantiate_reflection(value);
}

} // namespace negative_reflected_call_template_callstack
