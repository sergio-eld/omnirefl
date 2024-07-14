#include "o1.h"

namespace ignore_defined_in_scope {
std::string deserialize(const ignored_in_cpp_def_1 &) {
  return "ignored_in_cpp_def_1";
}
} // namespace ignore_defined_in_scope

std::string outer::inner::deserialize(const outer::ignored_in_cpp_def_1 &) {
  return "outer::ignored_in_cpp_def_1";
}

void ignoreme();
