#include <omnirefl/reflection.hpp>

namespace negative_query_out_of_scope {
struct record {
  int value;
};

static_assert(!omni::is_reflected<record>::value,
  "out-of-scope reflection query should be rejected by the tool");
} // namespace negative_query_out_of_scope
