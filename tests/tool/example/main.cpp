#include <omnirefl/functional.hpp>
#include <omnirefl/reflection.hpp>

#include <iostream>
#include <string>
#include <string_view>

struct record {
  int foo;
  std::string bar;
};

int main() {
  using namespace std::string_view_literals;

  record value{
    .foo = 1,
    .bar = "before",
  };

  std::cout << "before: foo=" << value.foo << " bar=" << value.bar << '\n';

  const auto write = [](omni::binding auto b)
    // Generic lambdas used as reflected visitors must spell the return type.
    -> void {
      // `omni::fn::each` is the QoL equivalent of expanding a visitor over a
      // tuple with `std::apply`.
      omni::fn::each(
        [](omni::field_binding auto field) -> void {
          constexpr std::string_view name = field.name();

          // value() is read-only; ref() exposes a writable reference.
          if constexpr ("foo"sv == name)
            field.ref() = 815;

          // operator* and operator-> are QoL accessors.
          if constexpr ("bar"sv == name)
            *field = "oceanic";
        },
        b.public_fields());
    };

  omni::reflected_call(write, value);

  std::cout << "after: foo=" << value.foo << " bar=" << value.bar << '\n';
}
