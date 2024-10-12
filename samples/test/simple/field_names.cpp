#include "simple/structs.h"

#include <iostream>
#include <string>
#include <vector>

#include <omnirefl/refl.h>

// todo: implement
int main() {
  const example_types::person p{
    "JohnCena",
    47,
    "You can't see me",
  };
  std::vector<std::string> result;
  omni::reflected_call(example_impl::print_field_names, p, result);
  for (const auto &s : result)
    std::cout << s << '\n';

  return 0;
}
