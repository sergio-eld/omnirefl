// Expected failure: scalar, standard container, pointer, and array arguments
// are not reflectable top-level reflected_call inputs.
#include <omnirefl/reflected_scope.hpp>

#include <vector>

namespace negative_unsupported_arguments {

struct record {
  int value;
};

void run() {
  const auto visit = [](auto...) -> void {};

  int scalar = 0;
  std::vector<int> vector;
  record r{1};
  record *ptr = &r;
  int raw_array[2] = {1, 2};

  (void)omni::reflected_call(visit,
    scalar,
    vector,
    ptr,
    raw_array);
}

} // namespace negative_unsupported_arguments
