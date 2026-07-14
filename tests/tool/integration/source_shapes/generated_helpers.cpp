#include <omnirefl/reflection.hpp>

struct record {
  int value;
};

struct read_value {
  template <typename Binding>
  int operator()(Binding binding) const {
    return binding.value.value;
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
