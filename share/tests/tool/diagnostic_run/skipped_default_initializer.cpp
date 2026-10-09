// Expected warnings: calls and macros are not replayed.
#include <gtest/gtest.h>
#include <omnirefl/reflected_scope.hpp>

#define OMNI_TEST_DEFAULT_VALUE 813

namespace diagnostic_skipped_default_initializer {

namespace defaults {

inline int value() {
  return 815;
}

} // namespace defaults

using namespace defaults;

struct record {
  int field = value();
  int expanded = 1 + OMNI_TEST_DEFAULT_VALUE + 1;
};

} // namespace diagnostic_skipped_default_initializer

TEST(diagnostic, skipped_default_initializer) {
  using diagnostic_skipped_default_initializer::record;

  omni::reflected_call(
    [](auto record_meta) -> void {
      omni::compat::apply(
        [](auto field, auto expanded) -> void {
          using field_meta = decltype(field);
          using expanded_meta = decltype(expanded);

          static_assert(field_meta::has_default_member_initializer(),
            "the declaration fact must remain available");
          static_assert(!field_meta::has_default_value_access(),
            "a lookup-dependent initializer must not be replayed");
          static_assert(expanded_meta::has_default_member_initializer(),
            "the macro-initialized declaration must remain available");
          static_assert(!expanded_meta::has_default_value_access(),
            "a nested macro expansion must not be replayed");
        },
        record_meta.public_fields());
    },
    omni::type_t<record>{});
}

#undef OMNI_TEST_DEFAULT_VALUE
