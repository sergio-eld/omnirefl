#include <omnirefl/reflection.hpp>

extern "C" int helper(void);

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
  return 7 == helper() && 19 == omni::reflected_call(read_value{}, record{19})
    ? 0
    : 1;
}
