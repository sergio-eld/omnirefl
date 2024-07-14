#include "o2.h"

namespace ignore_defined_in_scope {
std::string deserialize(const ignored_in_cpp_def_2 &) {
  return "ignored_in_cpp_def_2";
}
} // namespace ignore_defined_in_scope

std::string outer::inner::deserialize(const outer::ignored_in_cpp_def_2 &) {
  return "outer::ignored_in_cpp_def_2";
}
