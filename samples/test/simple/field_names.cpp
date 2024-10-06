#include "simple/structs.h"

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

  return 0;
}
