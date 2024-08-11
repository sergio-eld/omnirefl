#pragma once

#include <string>

struct ignored_in_header_def_1 {};
inline std::string serialize(const ignored_in_header_def_1 &);
inline std::string serialize(const ignored_in_header_def_1 &) {
  return "ignored_in_header_def";
}

namespace ignore_defined_in_scope {
struct ignored_in_cpp_def_1 {};
std::string serialize(const ignored_in_cpp_def_1 &);
} // namespace ignore_defined_in_scope

namespace outer {
struct undefined_a_1 {
  std::string a;
  int b;
};

namespace inner {
struct ignored_in_cpp_def_1 {};
std::string serialize(const ignored_in_cpp_def_1 &);

struct undefined_b_1 {
  std::string s;
};
} // namespace inner
} // namespace outer

