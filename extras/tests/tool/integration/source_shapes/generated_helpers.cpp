#include <omnirefl/reflected_scope.hpp>

struct record {
  int number;
};

struct read_value {
  template <typename Binding>
  int operator()(Binding binding) const {
    return binding.ref().number;
  }
};

int generated_absolute();
int generated_relative();

int main() {
  return 23 == omni::reflected_call(read_value{}, record{23})
      && 11 == generated_absolute() && 12 == generated_relative()
    ? 0
    : 1;
}
