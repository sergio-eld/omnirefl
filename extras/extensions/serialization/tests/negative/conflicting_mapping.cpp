// Expected failure: both cp::to_type_t and cp::to_type register int.
// Selection must diagnose conflicting registrations, not an invalid mapper.
#include <omnirefl/serialization/ryml.hpp>

namespace omni {
namespace ryml {
namespace cp {

template <>
struct to_type_t<int> {
  // Delegate because the mapper is final: `.tdm_` updates return its own type
  // and cannot preserve members added by derived registrations.
  static constexpr auto arithmetic_mapping()
    -> decltype(map_arithmetic_node_t<>{}) {
    return map_arithmetic_node_t<>{};
  }

  template <typename... Args>
  auto operator()(Args &&...args) const
    -> decltype(arithmetic_mapping()(std::forward<Args>(args)...)) {
    constexpr auto mapping = arithmetic_mapping();
    return mapping(std::forward<Args>(args)...);
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
