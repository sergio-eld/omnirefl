#pragma once

#include <string>
#include <vector>

// For this structure an identicall `_call_impl` will be generated in the reflection .cpp file for
// each target. This test is intended to check for no ODR violation.
namespace odr {
struct dummy {
  int oceanic;
};

std::vector<std::string> get_field_names_a(const dummy &);
std::vector<std::string> get_field_names_b(const dummy &);
} // namespace odr
