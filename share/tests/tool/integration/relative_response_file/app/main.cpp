#if !defined(OMNI_RESPONSE_FILE_EXPANDED)
#  error "relative compiler response file was not expanded"
#endif

#include <omnirefl/reflected_scope.hpp>

struct record {
  int number;
};

int main() {
  return omni::reflected_call(
    [](auto binding) -> int { return binding.ref().number; },
    record{0});
}
