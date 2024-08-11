#include "o1.h"

namespace ignore_defined_in_scope {
std::string serialize(const ignored_in_cpp_def_1 &) {
  return "ignored_in_cpp_def_1";
}
} // namespace ignore_defined_in_scope

std::string outer::inner::serialize(const ignored_in_cpp_def_1 &) {
  return "outer::ignored_in_cpp_def_1";
}

void ignoreme();
