#include "convert.hpp"

#include <utility>

namespace aggregate_missing_field {

struct source {
  int id;
};

struct destination {
  int id;
  int missing;
};

} // namespace aggregate_missing_field

int main() {
  namespace aggregate = aggregate_missing_field;

  aggregate::source source{815};
  (void)omni::reflected_call(aggregate_test::convert{},
    std::move(source),
    omni::type_t<aggregate::destination>{});
}
