#include <omnirefl/reflection.hpp>

namespace negative_reflected_dependency_private_nested {

struct record {
private:
  struct hidden {
    int value;
  };

public:
  hidden value;
};

void run() {
  record r{};

  (void)omni::reflected_call(
    [](auto) -> void {},
    r);
}

} // namespace negative_reflected_dependency_private_nested
