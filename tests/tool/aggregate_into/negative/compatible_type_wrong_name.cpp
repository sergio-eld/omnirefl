// Expected failure: a compatible source field with a different name does not
// satisfy the destination field.
#include "../convert.hpp"

#include <utility>

namespace aggregate_compatible_type_wrong_name {

struct source {
  int other;
};

struct destination {
  int value;
};

} // namespace aggregate_compatible_type_wrong_name

int main() {
  namespace aggregate = aggregate_compatible_type_wrong_name;

  aggregate::source source{815};
  (void)omni::reflected_call(aggregate_test::convert{},
    std::move(source),
    omni::type_t<aggregate::destination>{});
}
