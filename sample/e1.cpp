
#include <omnirefl/refl.h>

#include "o1.h"
#include "o2.h"

#include <iostream>

int main() {
  std::cout << omni::serialize(ignored_in_header_def_1{}) << '\n';
  std::cout << omni::serialize(ignored_in_header_def_1{}) << '\n';
  std::cout << omni::serialize(ignored_in_header_def_2{}) << '\n';
  std::cout << omni::serialize(ignore_defined_in_scope::ignored_in_cpp_def_1{}) << '\n';
  std::cout << omni::serialize(outer::inner::ignored_in_cpp_def_2{}) << '\n';

  // todo: make them work
  // todo: gtest
  const auto& ua1 = omni::deserialize.to<outer::undefined_a_1>(
          ryml::parse_in_arena(R"({"a": "string", "b": 815})"));
  std::cout << ua1->a << ", " << ua1->b << "\n";
  // todo: support these
  // std::cout << omni::serialize(outer::inner::undefined_b_1{}) << '\n';
  // std::cout << omni::serialize(outer::undefined_a_2{}) << '\n';
  // std::cout << omni::serialize(outer::inner::undefined_b_2{}) << '\n';
  // todo: add test for local unnamed structure

  return 0;
}
