/** Compile-cost probe for a wide, nonrecursive record with 100 optional fields.
 * Build `cpp_<standard>.benchmark.record_shape_compile_cost` explicitly.
 * Targets are excluded from default builds and CI. Measure compiler time and
 * peak memory separately from reflection generation. Run it to check
 * every field survives JSON deserialization and serialization.
 *
 * Suffix tuples in `try_fold` and events containing the whole record frame type
 * caused excessive compiler memory use. This shape also catches doubled depth
 * when presence checks and field conversions share one 200-element fold.
 * Clang 6 with libstdc++ 7 needs `-ftemplate-depth=2048` for metadata tuples.
 */
#include <omnirefl/serialization/ryml.hpp>

#include <iostream>
#include <string>

namespace compile_cost {

struct record {
#define OMNI_COMPILE_COST_MEMBER(index) omni::compat::optional<int> m##index;
  OMNI_COMPILE_COST_MEMBER(0)  OMNI_COMPILE_COST_MEMBER(1)
  OMNI_COMPILE_COST_MEMBER(2)  OMNI_COMPILE_COST_MEMBER(3)
  OMNI_COMPILE_COST_MEMBER(4)  OMNI_COMPILE_COST_MEMBER(5)
  OMNI_COMPILE_COST_MEMBER(6)  OMNI_COMPILE_COST_MEMBER(7)
  OMNI_COMPILE_COST_MEMBER(8)  OMNI_COMPILE_COST_MEMBER(9)
  OMNI_COMPILE_COST_MEMBER(10)  OMNI_COMPILE_COST_MEMBER(11)
  OMNI_COMPILE_COST_MEMBER(12)  OMNI_COMPILE_COST_MEMBER(13)
  OMNI_COMPILE_COST_MEMBER(14)  OMNI_COMPILE_COST_MEMBER(15)
  OMNI_COMPILE_COST_MEMBER(16)  OMNI_COMPILE_COST_MEMBER(17)
  OMNI_COMPILE_COST_MEMBER(18)  OMNI_COMPILE_COST_MEMBER(19)
  OMNI_COMPILE_COST_MEMBER(20)  OMNI_COMPILE_COST_MEMBER(21)
  OMNI_COMPILE_COST_MEMBER(22)  OMNI_COMPILE_COST_MEMBER(23)
  OMNI_COMPILE_COST_MEMBER(24)  OMNI_COMPILE_COST_MEMBER(25)
  OMNI_COMPILE_COST_MEMBER(26)  OMNI_COMPILE_COST_MEMBER(27)
  OMNI_COMPILE_COST_MEMBER(28)  OMNI_COMPILE_COST_MEMBER(29)
  OMNI_COMPILE_COST_MEMBER(30)  OMNI_COMPILE_COST_MEMBER(31)
  OMNI_COMPILE_COST_MEMBER(32)  OMNI_COMPILE_COST_MEMBER(33)
  OMNI_COMPILE_COST_MEMBER(34)  OMNI_COMPILE_COST_MEMBER(35)
  OMNI_COMPILE_COST_MEMBER(36)  OMNI_COMPILE_COST_MEMBER(37)
  OMNI_COMPILE_COST_MEMBER(38)  OMNI_COMPILE_COST_MEMBER(39)
  OMNI_COMPILE_COST_MEMBER(40)  OMNI_COMPILE_COST_MEMBER(41)
  OMNI_COMPILE_COST_MEMBER(42)  OMNI_COMPILE_COST_MEMBER(43)
  OMNI_COMPILE_COST_MEMBER(44)  OMNI_COMPILE_COST_MEMBER(45)
  OMNI_COMPILE_COST_MEMBER(46)  OMNI_COMPILE_COST_MEMBER(47)
  OMNI_COMPILE_COST_MEMBER(48)  OMNI_COMPILE_COST_MEMBER(49)
  OMNI_COMPILE_COST_MEMBER(50)  OMNI_COMPILE_COST_MEMBER(51)
  OMNI_COMPILE_COST_MEMBER(52)  OMNI_COMPILE_COST_MEMBER(53)
  OMNI_COMPILE_COST_MEMBER(54)  OMNI_COMPILE_COST_MEMBER(55)
  OMNI_COMPILE_COST_MEMBER(56)  OMNI_COMPILE_COST_MEMBER(57)
  OMNI_COMPILE_COST_MEMBER(58)  OMNI_COMPILE_COST_MEMBER(59)
  OMNI_COMPILE_COST_MEMBER(60)  OMNI_COMPILE_COST_MEMBER(61)
  OMNI_COMPILE_COST_MEMBER(62)  OMNI_COMPILE_COST_MEMBER(63)
  OMNI_COMPILE_COST_MEMBER(64)  OMNI_COMPILE_COST_MEMBER(65)
  OMNI_COMPILE_COST_MEMBER(66)  OMNI_COMPILE_COST_MEMBER(67)
  OMNI_COMPILE_COST_MEMBER(68)  OMNI_COMPILE_COST_MEMBER(69)
  OMNI_COMPILE_COST_MEMBER(70)  OMNI_COMPILE_COST_MEMBER(71)
  OMNI_COMPILE_COST_MEMBER(72)  OMNI_COMPILE_COST_MEMBER(73)
  OMNI_COMPILE_COST_MEMBER(74)  OMNI_COMPILE_COST_MEMBER(75)
  OMNI_COMPILE_COST_MEMBER(76)  OMNI_COMPILE_COST_MEMBER(77)
  OMNI_COMPILE_COST_MEMBER(78)  OMNI_COMPILE_COST_MEMBER(79)
  OMNI_COMPILE_COST_MEMBER(80)  OMNI_COMPILE_COST_MEMBER(81)
  OMNI_COMPILE_COST_MEMBER(82)  OMNI_COMPILE_COST_MEMBER(83)
  OMNI_COMPILE_COST_MEMBER(84)  OMNI_COMPILE_COST_MEMBER(85)
  OMNI_COMPILE_COST_MEMBER(86)  OMNI_COMPILE_COST_MEMBER(87)
  OMNI_COMPILE_COST_MEMBER(88)  OMNI_COMPILE_COST_MEMBER(89)
  OMNI_COMPILE_COST_MEMBER(90)  OMNI_COMPILE_COST_MEMBER(91)
  OMNI_COMPILE_COST_MEMBER(92)  OMNI_COMPILE_COST_MEMBER(93)
  OMNI_COMPILE_COST_MEMBER(94)  OMNI_COMPILE_COST_MEMBER(95)
  OMNI_COMPILE_COST_MEMBER(96)  OMNI_COMPILE_COST_MEMBER(97)
  OMNI_COMPILE_COST_MEMBER(98)  OMNI_COMPILE_COST_MEMBER(99)
#undef OMNI_COMPILE_COST_MEMBER
};

} // namespace compile_cost

int main() {
  std::string document{"{"};
  for (int i = 0; i < 100; ++i) {
    if (i)
      document += ',';
    document += "\"m" + std::to_string(i) + "\":" + std::to_string(i);
  }
  document += '}';

  const auto result = omni::ryml::deserialize(
    omni::type_t<compile_cost::record>{}, std::move(document));
  if (!result.value) {
    std::cerr << result.diagnostics << '\n';
    return 1;
  }

  const auto serialized = omni::ryml::core::parse(
    omni::ryml::as_json(*result.value));
  if (!serialized) {
    std::cerr << serialized.error() << '\n';
    return 1;
  }

  const auto object = serialized->crootref();
  if (object.num_children() != 100) {
    std::cerr << "Expected 100 fields, got " << object.num_children() << '\n';
    return 1;
  }
  for (int i = 0; i < 100; ++i) {
    const auto key = "m" + std::to_string(i);
    const auto field = object.find_child(c4::to_csubstr(key));
    if (field.invalid() || field.val() != c4::to_csubstr(std::to_string(i))) {
      std::cerr << "Incorrect round trip for " << key << '\n';
      return 1;
    }
  }
}
