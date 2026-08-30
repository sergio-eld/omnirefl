// Expected failure: supplying a missing optional field does not satisfy the
// remaining missing non-optional field.
#include "../completion.hpp"

#include <utility>

namespace aggregate_missing_after_optional_completion {

struct source {
  int id;
};

struct destination {
  int id;
  ts::optional<int> note;
  int count;
};

struct convert {
  template <typename From, typename To>
  constexpr To operator()(omni::binding_t<From> from,
    omni::meta_t<To> target) const {
    return omni::refl::aggregate_into<To>(omni::fn::concat(
      from.public_fields(),
      target.public_fields() //
        | omni::fn::diff_by(omni::fn::field_name{}, from.public_fields())
        | omni::fn::filter(aggregate_test::optional_field{})
        | omni::fn::map(aggregate_test::supply_missing{})));
  }
};

} // namespace aggregate_missing_after_optional_completion

int main() {
  namespace aggregate = aggregate_missing_after_optional_completion;

  (void)omni::reflected_call(aggregate::convert{},
    aggregate::source{815},
    omni::type_t<aggregate::destination>{});
}
