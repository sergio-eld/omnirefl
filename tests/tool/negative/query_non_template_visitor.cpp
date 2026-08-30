// Expected failure: a concrete visitor signature queries generated binding and
// metadata types before reflected_call establishes a reflected scope.
#include <omnirefl/reflection.hpp>

namespace negative_query_non_template_visitor {
struct record {
  int value;
};

struct visitor {
  int operator()(omni::binding_t<record>) const {
    return sizeof(omni::meta_t<record>);
  }
};

void run() {
  record r{1};
  (void)omni::reflected_call(visitor{}, r);
}
} // namespace negative_query_non_template_visitor
