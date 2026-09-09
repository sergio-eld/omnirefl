// Expected failure: a non-template record nested in a template record remains
// unsupported as a reflection input.
#include <omnirefl/reflected_scope.hpp>

namespace negative_non_template_nested_in_template {

template <typename T>
struct outer {
  struct inner {
    T value;
  };
};

} // namespace negative_non_template_nested_in_template

int main() {
  using namespace negative_non_template_nested_in_template;

  outer<int>::inner value{};
  omni::reflected_call([](auto) -> void {}, value);
}
