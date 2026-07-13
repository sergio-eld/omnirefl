#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <tuple>

namespace regression_primary_template_type_only {

template <typename T>
struct record {
  T value;
};

} // namespace regression_primary_template_type_only

// FIXME(high): passing a concrete primary-template specialization only through
// omni::type<T> leaves Clang's specialization declaration uninstantiated. The
// tool mistakes that state for a missing definition despite the visible
// primary definition.
TEST(regression,
  DISABLED_primary_template_type_argument_uses_visible_definition) {
  EXPECT_EQ(1,
    omni::reflected_call(
      [](auto meta) -> std::size_t {
        return std::tuple_size<decltype(meta.public_fields())>::value;
      },
      omni::type<regression_primary_template_type_only::record<int>>));
}
