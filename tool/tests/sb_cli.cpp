
#include "tool/cli.hpp"

#include <tl/expected.hpp>

#include <iostream>

int main(int argc, char **argv) {
  tl::expected parsed = tool::cli::parse(argc, argv);
  if (!parsed) {
    const auto &[msg, code] = parsed.error();
    std::cerr << msg << '\n';
    return code;
  }

  tl::expected evaluated =
    tool::cli::evaluate_defaults(std::move(parsed).value());
  if (!evaluated) {
    std::cerr << evaluated.error() << '\n';
    return -1;
  }

  std::cout << tool::cli::to_string(*evaluated) << '\n';
  return 0;
}
