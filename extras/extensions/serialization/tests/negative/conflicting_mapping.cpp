#include <omnirefl/serialization/ryml.hpp>

namespace omni {
namespace ryml {
namespace cp {

template <>
struct to_type_t<int>: map_arithmetic_node_t<> {};

template <>
constexpr auto to_type<int> = map_arithmetic_node;

} // namespace cp
} // namespace ryml
} // namespace omni

int main() {
  (void)omni::ryml::reflected_scope::mapping_to_type(omni::type_t<int>{});
}
