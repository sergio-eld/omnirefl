#include <omnirefl/serialization/ryml.hpp>

struct unregistered {};

int main() {
  (void)omni::ryml::to_type(omni::type_t<unregistered>{});
}
