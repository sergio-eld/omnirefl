#include <gtest/gtest.h>
#include <omnirefl/reflection.hpp>

#include <string>
#include <tuple>

namespace regression_nested_template_field_name {

struct outer {
  template <typename T>
  struct nested {
    T value;
  };
};

struct pointer_record {
  outer::nested<int> *value;
};

struct field_type_name {
  template <typename Meta>
  std::string operator()(Meta meta) const {
    return std::get<0>(meta.public_fields()).type_name();
  }
};

} // namespace regression_nested_template_field_name

// FIXME(medium): wrapped template TypeLocs fall back to string shortening,
// which removes both namespace and enclosing-record qualifiers.
TEST(regression,
  DISABLED_pointer_wrapped_nested_template_keeps_enclosing_record_name) {
  using namespace regression_nested_template_field_name;

  EXPECT_EQ("outer::nested<int> *",
    omni::reflected_call(field_type_name{}, omni::type_t<pointer_record>{}));
}
