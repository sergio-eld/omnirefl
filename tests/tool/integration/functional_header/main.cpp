#include <omnirefl/functional.hpp>

#include <string>
#include <tuple>
#include <type_traits>

struct integral {
  template <typename Element>
  constexpr bool operator()() const {
    return std::is_integral<
      omni::compat::remove_cvref_t<Element>>::value;
  }
};

struct double_value {
  int operator()(int value) const {
    return value * 2;
  }
};

struct add {
  int operator()(int left, int right) const {
    return left + right;
  }
};

int main() {
  const std::tuple<int, std::string, int> values{20, "ignored", 22};
  const auto result = values
    | omni::fn::filter(integral{})
    | omni::fn::map(double_value{})
    | omni::fn::foldl(add{}, 0);

  return 84 == result ? 0 : 1;
}
