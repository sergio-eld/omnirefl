// FIXME: Consider making visitor-local types reflectable. This disabled case
// currently demonstrates the unavailable-metadata failure instead.
#include <omnirefl/reflection.hpp>

namespace reflected_negative {

struct root {
  int value;
};

struct query {
  template <typename T>
  void operator()(omni::record_binding_t<T>) const {
    struct unavailable {};

    static_assert(!omni::is_reflected<unavailable>::value,
      "unavailable must remain queryable without becoming reflected");
    (void)omni::reflected(omni::type_t<unavailable>{});
  }
};

} // namespace reflected_negative

int main() {
  omni::reflected_call(reflected_negative::query{}, reflected_negative::root{});
}
