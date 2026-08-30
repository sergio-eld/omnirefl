// Expected failure: a reflection query cannot determine a generic visitor's
// return type when that type is deduced from the visitor body.
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
