// Expected failure: aggregate_into rejects destinations with base classes.
#include "../convert.hpp"

#include <utility>

namespace aggregate_base_class {

struct base {
  int inherited;
};

struct source {
  int inherited;
  int own;
};

struct destination: base {
  int own;
};

} // namespace aggregate_base_class

int main() {
  namespace aggregate = aggregate_base_class;

  aggregate::source source{815, 47};
  (void)omni::reflected_call(aggregate_test::convert{},
    std::move(source),
    omni::type_t<aggregate::destination>{});
}
