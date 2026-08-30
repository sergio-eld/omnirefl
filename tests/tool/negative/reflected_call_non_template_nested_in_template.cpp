// Expected failure: a non-template record nested in a template record remains
// unsupported as a reflection input.
#include <omnirefl/reflection.hpp>

namespace negative_reflected_call_non_template_nested_in_template {

template <typename T>
struct outer {
  struct inner {
    T value;
  };
};

} // namespace negative_reflected_call_non_template_nested_in_template

int main() {
  using namespace negative_reflected_call_non_template_nested_in_template;

  outer<int>::inner value{};
  omni::reflected_call([](auto) -> void {}, value);
}
