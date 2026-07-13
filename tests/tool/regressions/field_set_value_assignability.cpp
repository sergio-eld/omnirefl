#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace regression_field_set_value_assignability {

struct nonassignable {
  nonassignable() = default;
  nonassignable(const nonassignable &) = delete;
  nonassignable &operator=(const nonassignable &) = delete;
};

struct record {
  int raw[2];
  nonassignable fixed;
};

template <typename Field, typename Value, typename = void>
struct can_set : std::false_type {};

template <typename Field, typename Value>
struct can_set<Field,
  Value,
  omni::compat::void_t<decltype(std::declval<Field &>().set_value(
    std::declval<Value>()))>> : std::true_type {};

} // namespace regression_field_set_value_assignability

// FIXME(medium): set_value is constrained by owner constness but not by field
// assignability, so its interface advertises writes whose bodies cannot
// compile.
TEST(regression, DISABLED_set_value_requires_assignable_field_and_value) {
  using namespace regression_field_set_value_assignability;
  record input{{1, 2}, {}};

  EXPECT_TRUE(omni::reflected_call(
    [](auto binding) -> bool {
      return omni::compat::apply(
        [](auto... field) -> bool {
          const auto valid = [](auto item) -> bool {
            using field_t = decltype(item);
            return "raw" == std::string_view{item.name()}
              ? !can_set<field_t, int (&)[2]>::value
              : !can_set<field_t, nonassignable &&>::value;
          };
          return (valid(field) && ...);
        },
        binding.public_fields());
    },
    input));
}
