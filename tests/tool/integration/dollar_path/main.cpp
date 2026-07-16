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

int main() {
  return omni::reflected_call(read_value{}, record{});
}
