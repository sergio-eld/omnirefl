#if !defined(OMNI_RESPONSE_FILE_EXPANDED)
#  error "relative compiler response file was not expanded"
#endif

#include <omnirefl/reflection.hpp>

struct record {
  int value;
};

int main() {
  return omni::reflected_call(
    [](auto binding) -> int { return binding.record.value; },
    record{0});
}
