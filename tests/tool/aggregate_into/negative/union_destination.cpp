// Expected failure: aggregate_into rejects union destinations.
#include "../convert.hpp"

#include <utility>

namespace aggregate_union_destination {

struct source {
  int value;
};

union destination {
  int value;
};

} // namespace aggregate_union_destination

int main() {
  namespace aggregate = aggregate_union_destination;

  aggregate::source source{815};
  (void)omni::reflected_call(aggregate_test::convert{},
    std::move(source),
    omni::type_t<aggregate::destination>{});
}
