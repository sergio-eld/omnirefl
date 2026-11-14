#include <omnirefl/serialization/ryml.hpp>

namespace omni {
namespace ryml {
namespace cp {

template <>
struct to_type_t<int>: to_arithmetic {};

template <>
constexpr auto to_type<int> = to_arithmetic{};

} // namespace cp
} // namespace ryml
} // namespace omni

int main() {
  (void)omni::ryml::to_type(omni::type_t<int>{});
}
