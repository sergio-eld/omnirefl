// Expected failure: a record nested in a template record cannot be emitted as
// supported reflection metadata.
#include <omnirefl/reflection.hpp>

namespace negative_reflected_call_nested_template_parent {

template <typename Outer>
struct parent {
  template <typename Inner>
  struct nested {
    Outer outer;
    Inner inner;
  };
};

void run() {
  parent<int>::nested<long> value{};

  (void)omni::reflected_call([](auto) -> void {}, value);
}

} // namespace negative_reflected_call_nested_template_parent
