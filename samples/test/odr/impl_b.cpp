
#include "get_field_names.hpp"

std::vector<std::string> odr::get_field_names_b(const dummy &d) {
  std::vector<std::string> result;
  omni::reflected_call(odr::print_field_names, d, result);
  return result;
}
