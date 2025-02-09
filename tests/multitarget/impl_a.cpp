
#include "get_field_names.hpp"

#include "print_field_names.hpp"

#include <omnirefl/refl.hpp>

std::vector<std::string> odr::get_field_names_a(const dummy &d) {
  std::vector<std::string> result;
  omni::reflected_call(odr::print_field_names, d, result);
  return result;
}
