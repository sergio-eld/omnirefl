#include <omnirefl/reflection.hpp>

struct record {
  int value;
};

struct read_value {
  template <typename Binding>
  int operator()(Binding binding) const {
    return binding.record.value;
  }
};

int main() {
  return 47 == omni::reflected_call(read_value{}, record{47}) ? 0 : 1;
}
