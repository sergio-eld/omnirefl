// Expected failure: reflected metadata and bindings are requested outside a
// reflected scope.
#include <omnirefl/reflected_scope.hpp>

namespace negative_query_functions_out_of_scope {
struct record {
  int value;
};

void run() {
  (void)omni::reflected(omni::type_t<record>{});

  record value{1};
  (void)omni::reflected(value);
}
} // namespace negative_query_functions_out_of_scope
