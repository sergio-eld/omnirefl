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
    /*titles=*/
    {
      {"WWE Championship", "16-time champion"},
      {"World Heavyweight Championship", "3-time champion"},
      {"United States Championship", "5-time champion"},
      {"Royal Rumble", "2-time winner"},
      {"Money in the Bank", "1-time winner"},
      {"Tag Team Championship", "4-time champion"},
    },
  };
  std::vector<std::string> result;
  omni::reflected_call(example_impl::print_field_names, p, result);
  for (const auto &s : result)
    std::cout << s << '\n';

  return 0;
}
