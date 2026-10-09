// Expected failure: the same-named source field cannot construct the
// destination field.
#include "../convert.hpp"

#include <string>
#include <utility>

namespace aggregate_incompatible_field {

struct source {
  std::string id;
};

struct destination {
  int id;
};

} // namespace aggregate_incompatible_field

int main() {
  namespace aggregate = aggregate_incompatible_field;

  aggregate::source source{"815"};
  (void)omni::reflected_call(aggregate_test::convert{},
    std::move(source),
    omni::type_t<aggregate::destination>{});
}
