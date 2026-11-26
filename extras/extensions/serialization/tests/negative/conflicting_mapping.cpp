// Expected failure: both cp::to_type_t and cp::to_type register int.
// Selection must diagnose conflicting registrations, not an invalid mapper.
#include <omnirefl/serialization/ryml.hpp>

namespace omni {
namespace ryml {
namespace cp {

template <>
struct to_type_t<int> {
  // Delegate to a valid final mapper so only registration conflicts fail.
  map_arithmetic_node_t<> arithmetic_mapping{};

  template <typename... Args>
  auto operator()(Args &&...args) const
    -> decltype(this->arithmetic_mapping(std::forward<Args>(args)...)) {
    return this->arithmetic_mapping(std::forward<Args>(args)...);
  }
};

template <>
constexpr auto to_type<int> = map_arithmetic_node;

} // namespace cp
} // namespace ryml
} // namespace omni

int main() {
  (void)omni::ryml::reflected_scope::mapping_to_type(omni::type_t<int>{});
}
