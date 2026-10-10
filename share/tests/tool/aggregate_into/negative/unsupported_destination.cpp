// Expected failure: aggregate_into rejects a non-aggregate destination.
#include "../convert.hpp"

#include <utility>

namespace aggregate_unsupported_destination {

struct source {};

struct destination {
  destination() {}
};

} // namespace aggregate_unsupported_destination

int main() {
  namespace aggregate = aggregate_unsupported_destination;

  aggregate::source source;
  (void)omni::reflected_call(aggregate_test::convert{},
    std::move(source),
    omni::type_t<aggregate::destination>{});
}
