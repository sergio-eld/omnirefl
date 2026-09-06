// Expected failure: one call combines an out-of-scope reflection query with a
// non-reflectable top-level argument.
#include <omnirefl/reflected_scope.hpp>

namespace negative_query_and_unsupported_argument {

struct record {
  int value;
};

static_assert(!omni::is_reflected<record>::value,
  "out-of-scope reflection query should be rejected by the tool");

void run() {
  int value = 0;

  (void)omni::reflected_call([](auto) -> void {}, value);
}

} // namespace negative_query_and_unsupported_argument
