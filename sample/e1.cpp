
#include "o1.h"
#include "o2.h"

#include <iostream>

int main() {
  std::cout << deserialize(ignored_in_header_def_1{}) << '\n';
  std::cout << deserialize(ignored_in_header_def_2{}) << '\n';
  std::cout << outer::inner::deserialize(outer::ignored_in_cpp_def_1{}) << '\n';
  std::cout << outer::inner::deserialize(outer::ignored_in_cpp_def_2{}) << '\n';

  std::cout << deserialize(outer::undefined_a_1{}) << '\n';
  std::cout << deserialize(outer::inner::undefined_b_1{}) << '\n';
  std::cout << deserialize(outer::undefined_a_2{}) << '\n';
  std::cout << deserialize(outer::inner::undefined_b_2{}) << '\n';

  return 0;
}
