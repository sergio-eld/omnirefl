// Expected failure: unregistered has neither a custom mapping nor a supported
// default. Selection must report "No node mapping: specialize".
#include <omnirefl/serialization/ryml.hpp>

struct unregistered {};

int main() {
  (void)omni::ryml::reflected_scope::mapping_to_type(
    omni::type_t<unregistered>{});
}
