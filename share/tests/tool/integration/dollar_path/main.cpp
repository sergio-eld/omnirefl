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

int main() {
  return omni::reflected_call(read_value{}, record{});
}
