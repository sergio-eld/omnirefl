#include <omnirefl/reflection.hpp>

namespace diagnostic_reflected_dependency_private_nested {

struct record {
  private:
  struct hidden {
    int value;
  };

  public:
  hidden value;
};

} // namespace diagnostic_reflected_dependency_private_nested

int main() {
  diagnostic_reflected_dependency_private_nested::record value{};
  omni::reflected_call([](auto) -> void {}, value);
}
