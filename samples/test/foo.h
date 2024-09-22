#pragma once

#include <string>
// fixme: older systems don't have C++17 support. 
// #include <tuple>
// #include <variant>
#include <vector>

// do not call serialization for this function directly, it should be generated when `bar` is
// deserialized
struct foo {
  std::string a;
  int b;
};

struct bar {
  double d;
  foo f;
};

struct with_vector {
  std::vector<foo> vec;
};

struct with_user_defined_serialization {
  std::string value;
};
// todo: ^^^ implement serialization

// validate generation for dependent types within `std::variant`
namespace dependent_types_variant {
// do not call deserialization directly
struct _1 {
  int a;
};
// do not call deserialization directly
struct _2 {
  double b;
};

// fixme: older systems don't have support for c++17
// // call deserialization, dependent types should be detected
// struct s {
//   std::variant<_1, _2> v;
// };
} // namespace dependent_types_variant

// todo: nested structs
// todo: forward declarations
// todo: reference fields + error on const fields
// todo: optional fields
