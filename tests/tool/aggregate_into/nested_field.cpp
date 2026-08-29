#include "convert.hpp"

#include <utility>

namespace aggregate_nested_field {

struct source_payload {
  int id;
};

struct destination_payload {
  int id;
};

struct source {
  source_payload payload;
};

struct destination {
  destination_payload payload;
};

} // namespace aggregate_nested_field

int main() {
  namespace aggregate = aggregate_nested_field;

  aggregate::source source{{815}};
  (void)omni::reflected_call(aggregate_test::convert{},
    std::move(source),
    omni::type_t<aggregate::destination>{});
}
