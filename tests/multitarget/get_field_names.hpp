#pragma once

#include <string>
#include <vector>

namespace odr {
struct dummy {
  int oceanic;
};

std::vector<std::string> get_field_names_a(const dummy &);
std::vector<std::string> get_field_names_b(const dummy &);
} // namespace odr
