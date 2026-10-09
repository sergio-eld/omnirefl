#include <omnirefl/reflected_scope.hpp>

extern "C" int helper(void);

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
  return 7 == helper() && 19 == omni::reflected_call(read_value{}, record{19})
    ? 0
    : 1;
}
