#pragma once

#include <string>

struct ignored_in_header_def_1 {};
inline std::string deserialize(const ignored_in_header_def_1 &);
inline std::string deserialize(const ignored_in_header_def_1 &) {
  return "ignored_in_header_def";
}

namespace ignore_defined_in_scope {
struct ignored_in_cpp_def_1 {};
std::string deserialize(const ignored_in_cpp_def_1 &);
} // namespace ignore_defined_in_scope

namespace outer {
struct ignored_in_cpp_def_1 {};

struct undefined_a_1 {
  std::string a;
  int b;
};

std::string deserialize(const undefined_a_1 &);

namespace inner {
std::string deserialize(const ignored_in_cpp_def_1 &);

struct undefined_b_1 {
  std::string s;
};
std::string deserialize(const undefined_b_1 &);
} // namespace inner
} // namespace outer

