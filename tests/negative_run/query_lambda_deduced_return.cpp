#include <omnirefl/reflection.hpp>

namespace negative_query_lambda_deduced_return {
struct record {
  int value;
};

void run() {
  record r{1};
  (void)omni::reflected_call(
    [](omni::binding_t<record>) {
      return sizeof(omni::meta_t<record>);
    },
    r);
}
} // namespace negative_query_lambda_deduced_return
