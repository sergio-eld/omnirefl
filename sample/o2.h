#pragma once

#include <string>

struct ignored_in_header_def_2 {};
inline std::string deserialize(const ignored_in_header_def_2 &);
inline std::string deserialize(const ignored_in_header_def_2 &) {
  return "ignored_in_header_def";
}

namespace ignore_defined_in_scope {
struct ignored_in_cpp_def_2 {};
std::string deserialize(const ignored_in_cpp_def_2 &);
} // namespace ignore_defined_in_scope

namespace outer {
struct ignored_in_cpp_def_2 {};

struct undefined_a_2 {
  std::string a;
  int b;
};

std::string deserialize(const undefined_a_2 &);

namespace inner {
std::string deserialize(const ignored_in_cpp_def_2 &);

struct undefined_b_2 {
  std::string s;
};
std::string deserialize(const undefined_b_2 &);
} // namespace inner
} // namespace outer

