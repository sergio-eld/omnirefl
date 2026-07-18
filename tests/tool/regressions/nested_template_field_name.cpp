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

struct double_pointer_record {
  outer::nested<int> **value;
};

namespace types {

struct outer {
  struct inner {
    struct plain {
      int value;
    };

    template <typename T, typename U>
    struct actual {
      T first;
      U second;
    };
  };
};

template <typename T, typename U>
struct templated_outer {
  struct inner {
    struct actual {
      T first;
      U second;
    };

    template <typename V>
    struct nested {
      V value;
    };
  };
};

} // namespace types

struct namespace_qualified_record {
  types::outer::inner::actual<int, long> *value;
};

struct namespace_qualified_plain_record {
  types::outer::inner::plain *value;
};

struct const_pointer_record {
  const types::outer::inner::plain *value;
};

struct const_inner_pointer_record {
  types::outer::inner::plain *const *value;
};

struct reference_record {
  types::outer::inner::plain &value;
};

struct array_record {
  types::outer::inner::plain value[2];
};

struct multidimensional_array_record {
  types::outer::inner::plain value[2][3];
};

struct enclosing_template_record {
  types::templated_outer<int, long>::inner::actual *value;
};

struct enclosing_and_leaf_template_record {
  types::templated_outer<int, long>::inner::nested<float> *value;
};

struct field_type_name {
  template <typename Meta>
  std::string operator()(Meta meta) const {
    return std::get<0>(meta.public_fields()).type_name();
  }
};

} // namespace regression_nested_template_field_name

TEST(regression, pointer_wrapped_nested_template_keeps_enclosing_record_name) {
  using namespace regression_nested_template_field_name;

  EXPECT_EQ("outer::nested<int> *",
    omni::reflected_call(field_type_name{}, omni::type_t<pointer_record>{}));
  EXPECT_EQ("outer::nested<int> **",
    omni::reflected_call(field_type_name{},
      omni::type_t<double_pointer_record>{}));
  EXPECT_EQ("outer::inner::actual<int, long> *",
    omni::reflected_call(field_type_name{},
      omni::type_t<namespace_qualified_record>{}));
  EXPECT_EQ("outer::inner::plain *",
    omni::reflected_call(field_type_name{},
      omni::type_t<namespace_qualified_plain_record>{}));
  EXPECT_EQ("const outer::inner::plain *",
    omni::reflected_call(field_type_name{},
      omni::type_t<const_pointer_record>{}));
  EXPECT_EQ("outer::inner::plain * const *",
    omni::reflected_call(field_type_name{},
      omni::type_t<const_inner_pointer_record>{}));
  EXPECT_EQ("outer::inner::plain &",
    omni::reflected_call(field_type_name{}, omni::type_t<reference_record>{}));
  EXPECT_EQ("outer::inner::plain[2]",
    omni::reflected_call(field_type_name{}, omni::type_t<array_record>{}));
  EXPECT_EQ("outer::inner::plain[2][3]",
    omni::reflected_call(field_type_name{},
      omni::type_t<multidimensional_array_record>{}));
  EXPECT_EQ("templated_outer<int, long>::inner::actual *",
    omni::reflected_call(field_type_name{},
      omni::type_t<enclosing_template_record>{}));
  EXPECT_EQ("templated_outer<int, long>::inner::nested<float> *",
    omni::reflected_call(field_type_name{},
      omni::type_t<enclosing_and_leaf_template_record>{}));
}
