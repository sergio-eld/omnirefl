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
  template <typename From, typename _M>
  constexpr typename omni::record_meta_t<_M>::reflected_type operator()(
    omni::record_binding_t<From> from,
    omni::record_meta_t<_M> target) const {
    return omni::refl::aggregate_into<
      typename omni::record_meta_t<_M>::reflected_type>(
      omni::fn::concat(from.public_fields(),
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
