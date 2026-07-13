#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <string_view>
#include <tuple>

namespace regression_nested_template_field_name {

struct outer {
  template <typename T>
  struct nested {
    T value;
  };
};

struct record {
  outer::nested<int> value;
};

} // namespace regression_nested_template_field_name

// FIXME(medium): namespace stripping also removes the enclosing record from a
// nested template field's namespace-free type name.
TEST(regression, DISABLED_nested_template_field_keeps_enclosing_record_name) {
  using namespace regression_nested_template_field_name;

  EXPECT_TRUE(omni::reflected_call(
    [](auto meta) -> bool {
      const auto field = std::get<0>(meta.public_fields());
      return "outer::nested<int>" == std::string_view{field.type_name()}
        && "regression_nested_template_field_name::outer::nested<int>"
          == std::string_view{field.qualified_type_name()};
    },
    omni::type<record>));
}
