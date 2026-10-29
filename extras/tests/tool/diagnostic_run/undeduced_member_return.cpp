#include <gtest/gtest.h>
#include <omnirefl/reflected_scope.hpp>

namespace diagnostic_undeduced_return {

struct payload {
  int value;
};

struct record {
  // This alias is not a discovery protocol; make's parameter discovers payload.
  using input = payload;

  // No definition: the return dependency cannot be deduced, but the input can.
  auto make(payload);
};

struct inspect {
  template <typename _M>
  bool operator()(omni::record_meta_t<_M> meta) const {
    using record = typename decltype(meta)::reflected_type;
    return omni::is_reflected<typename record::input>::value;
  }
};

} // namespace diagnostic_undeduced_return

TEST(diagnostic, undeduced_member_return_preserves_parameter_dependencies) {
  namespace example = diagnostic_undeduced_return;

  EXPECT_TRUE(omni::reflected_call(example::inspect{},
    omni::type_t<example::record>{}));
}
