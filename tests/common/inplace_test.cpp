
#include "gtest_include.h"
#include "inplace_structs.h"

#include <omnirefl/reflected_call.hpp>
#include <omnirefl/reflected_scope.hpp>

#include <mpark/variant.hpp>

#include <string>
#include <tuple>
#include <vector>

#if defined(__has_cpp_attribute)
#  if __has_cpp_attribute(maybe_unused) \
    && (!defined CXX_STANDARD || 17 <= CXX_STANDARD)
#    define OMNI_INPLACE_MAYBE_UNUSED [[maybe_unused]]
#  endif
#endif
#ifndef OMNI_INPLACE_MAYBE_UNUSED
#  if defined(__GNUC__) || defined(__clang__)
#    define OMNI_INPLACE_MAYBE_UNUSED __attribute__((unused))
#  else
#    define OMNI_INPLACE_MAYBE_UNUSED
#  endif
#endif

namespace example {
struct in_cpp_struct {
  std::string in_cpp_field_0;
  int in_cpp_field_1;
  double in_cpp_field_2;
};

struct in_cpp_derived_from_header_struct: in_header_struct {
  std::string in_cpp_derived_field_0;
  int in_cpp_derived_field_1;
  double in_cpp_derived_field_2;
};

struct in_cpp_derived_from_cpp_struct: in_cpp_struct {
  std::string in_cpp_derived_from_cpp_field_0;
  int in_cpp_derived_from_cpp_field_1;
  double in_cpp_derived_from_cpp_field_2;
};

struct in_cpp_multi_base: in_header_struct, in_cpp_struct {
  std::string in_cpp_multi_base_field_0;
  int in_cpp_multi_base_field_1;
  double in_cpp_multi_base_field_2;
};

struct in_cpp_private_base: private in_cpp_struct {
  std::string in_cpp_private_base_field_0;
  int in_cpp_private_base_field_1;
  double in_cpp_private_base_field_2;
};

struct in_cpp_protected_base: protected in_header_struct {
  std::string in_cpp_protected_base_field_0;
  int in_cpp_protected_base_field_1;
  double in_cpp_protected_base_field_2;
};

class in_cpp_mixed_access {
  std::string in_cpp_implicit_private_field;

  public:
  std::string in_cpp_public_field_0;
  int in_cpp_public_field_1;
  double in_cpp_public_field_2;

  private:
  int in_cpp_private_field OMNI_INPLACE_MAYBE_UNUSED;

  protected:
  double in_cpp_protected_field;
};

enum in_cpp_enum {
  in_cpp_enum_a,
  in_cpp_enum_b,
  in_cpp_enum_c,
};

enum class in_cpp_scoped_enum {
  in_cpp_scoped_enum_a,
  in_cpp_scoped_enum_b,
  in_cpp_scoped_enum_c,
};

enum in_cpp_fixed_enum : int {
  in_cpp_fixed_enum_a,
  in_cpp_fixed_enum_b,
  in_cpp_fixed_enum_c,
};

enum class in_cpp_scoped_enum_with_underlying : unsigned {
  in_cpp_scoped_enum_with_underlying_a,
  in_cpp_scoped_enum_with_underlying_b,
  in_cpp_scoped_enum_with_underlying_c,
};

struct in_cpp_mid_base: in_header_struct {
  std::string in_cpp_mid_field_0;
  int in_cpp_mid_field_1;
  double in_cpp_mid_field_2;
};

struct in_cpp_deep_derived: in_cpp_mid_base {
  std::string in_cpp_deep_field_0;
  int in_cpp_deep_field_1;
  double in_cpp_deep_field_2;
};

#if defined CXX_STANDARD && 11 < CXX_STANDARD
auto unnamed_returned_struct() {
  struct {
    int g_a = 4;
    double g_b = 8.15;
    std::string g_c = "returned";
  } s{};
  return s;
}

auto unnamed_returned_struct_with_header_base() {
  struct: in_header_struct {
    std::string returned_header_base_field_0;
    int returned_header_base_field_1;
    double returned_header_base_field_2;
  } s{};
  return s;
}

auto unnamed_returned_struct_with_cpp_base() {
  struct: in_cpp_struct {
    std::string returned_cpp_base_field_0;
    int returned_cpp_base_field_1;
    double returned_cpp_base_field_2;
  } s{};
  return s;
}

auto unnamed_returned_struct_with_multi_base() {
  struct: in_header_struct, in_cpp_struct {
    std::string returned_multi_base_field_0;
    int returned_multi_base_field_1;
    double returned_multi_base_field_2;
  } s{};
  return s;
}
#endif

} // namespace example

struct {
  std::string oceanic = "815";
  int station = 4;
  double bearing = 8.15;
} const unnamed_global{};

struct: example::in_header_struct {
  std::string unnamed_global_header_base_field_0;
  int unnamed_global_header_base_field_1;
  double unnamed_global_header_base_field_2;
} const unnamed_global_with_header_base OMNI_INPLACE_MAYBE_UNUSED{};

struct: example::in_cpp_struct {
  std::string unnamed_global_cpp_base_field_0;
  int unnamed_global_cpp_base_field_1;
  double unnamed_global_cpp_base_field_2;
} const unnamed_global_with_cpp_base OMNI_INPLACE_MAYBE_UNUSED{};

struct: example::in_header_struct, example::in_cpp_struct {
  std::string unnamed_global_multi_base_field_0;
  int unnamed_global_multi_base_field_1;
  double unnamed_global_multi_base_field_2;
} const unnamed_global_with_multi_base OMNI_INPLACE_MAYBE_UNUSED{};

enum {
  unnamed_global_enum_a,
  unnamed_global_enum_b,
  unnamed_global_enum_c,
} const unnamed_global_enum{};

namespace example_impl {
struct print_field_names_simple_t {
  // c++11 friendly visitor
  struct _visit {
    template <typename... F>
    std::vector<std::string> operator()(const F &...field) {
      return {std::string(field.name())...};
    }
  };

  template <typename T>
  std::vector<std::string> operator()(const T &t) const {
    static_assert(omni::is_reflected<T>::value, "");
    const auto fields = omni::reflected(t).public_fields();
    return omni::compat::apply(_visit{}, fields);
  }
} const static print_field_names_simple{};
} // namespace example_impl

TEST(print_names, in_cpp_local_unnamed_struct) {
  struct {
    std::string in_cpp_local_unnamed_field_0;
    int in_cpp_local_unnamed_field_1;
    double in_cpp_local_unnamed_field_2;
  } p{};

  ASSERT_EQ((std::vector<std::string>{
              "in_cpp_local_unnamed_field_0",
              "in_cpp_local_unnamed_field_1",
              "in_cpp_local_unnamed_field_2",
            }),
    omni::reflected_call(example_impl::print_field_names_simple, p));
}

// FIXME: does not compile with Clang: header-mode local unnamed indexed
// specializations drift after multiple local unnamed reflected types, and
// local unnamed structs with named bases also hit ambiguous named/indexed
// base metadata. Keep these as coverage routes until header mode
// generates Clang-stable indexed specializations.
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_base) {
//   struct: example::in_cpp_struct {
//     std::string in_cpp_local_unnamed_with_base_field_0;
//     int in_cpp_local_unnamed_with_base_field_1;
//     double in_cpp_local_unnamed_with_base_field_2;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "in_cpp_field_0",
//               "in_cpp_field_1",
//               "in_cpp_field_2",
//               "in_cpp_local_unnamed_with_base_field_0",
//               "in_cpp_local_unnamed_with_base_field_1",
//               "in_cpp_local_unnamed_with_base_field_2",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_header_base) {
//   struct: example::in_header_struct {
//     std::string in_cpp_local_unnamed_header_base_field_0;
//     int in_cpp_local_unnamed_header_base_field_1;
//     double in_cpp_local_unnamed_header_base_field_2;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "in_header_field_0",
//               "in_header_field_1",
//               "in_header_field_2",
//               "in_cpp_local_unnamed_header_base_field_0",
//               "in_cpp_local_unnamed_header_base_field_1",
//               "in_cpp_local_unnamed_header_base_field_2",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_multi_base) {
//   struct: example::in_header_struct, example::in_cpp_struct {
//     std::string in_cpp_local_unnamed_multi_base_field_0;
//     int in_cpp_local_unnamed_multi_base_field_1;
//     double in_cpp_local_unnamed_multi_base_field_2;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "in_header_field_0",
//               "in_header_field_1",
//               "in_header_field_2",
//               "in_cpp_field_0",
//               "in_cpp_field_1",
//               "in_cpp_field_2",
//               "in_cpp_local_unnamed_multi_base_field_0",
//               "in_cpp_local_unnamed_multi_base_field_1",
//               "in_cpp_local_unnamed_multi_base_field_2",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_scalar_mix_0) {
//   struct {
//     bool local_scalar_bool;
//     char local_scalar_char;
//     long local_scalar_long;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_scalar_bool",
//               "local_scalar_char",
//               "local_scalar_long",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_scalar_mix_1) {
//   struct {
//     unsigned int local_scalar_unsigned;
//     float local_scalar_float;
//     std::string local_scalar_string;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_scalar_unsigned",
//               "local_scalar_float",
//               "local_scalar_string",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_scalar_mix_2) {
//   struct {
//     short local_scalar_short;
//     long long local_scalar_long_long;
//     double local_scalar_double;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_scalar_short",
//               "local_scalar_long_long",
//               "local_scalar_double",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_pointer_mix_0) {
//   struct {
//     int *local_pointer_int;
//     const char *local_pointer_char;
//     double *local_pointer_double;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_pointer_int",
//               "local_pointer_char",
//               "local_pointer_double",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_pointer_mix_1) {
//   struct {
//     bool *local_pointer_bool;
//     char *local_pointer_mutable_char;
//     float *local_pointer_float;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_pointer_bool",
//               "local_pointer_mutable_char",
//               "local_pointer_float",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_std_vector_field_0) {
//   struct {
//     std::vector<int> local_vector_ints;
//     std::string local_vector_owner_name;
//     double local_vector_owner_score;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_vector_ints",
//               "local_vector_owner_name",
//               "local_vector_owner_score",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_std_vector_field_1) {
//   struct {
//     std::vector<std::string> local_vector_strings;
//     int local_vector_owner_id;
//     float local_vector_owner_ratio;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_vector_strings",
//               "local_vector_owner_id",
//               "local_vector_owner_ratio",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_tuple_field_0) {
//   struct {
//     std::tuple<int, double> local_tuple_pair;
//     std::string local_tuple_owner_name;
//     int local_tuple_owner_count;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_tuple_pair",
//               "local_tuple_owner_name",
//               "local_tuple_owner_count",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_tuple_field_1) {
//   struct {
//     std::tuple<std::string, int> local_tuple_named_id;
//     float local_tuple_weight;
//     bool local_tuple_enabled;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_tuple_named_id",
//               "local_tuple_weight",
//               "local_tuple_enabled",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_variant_field_0) {
//   struct {
//     mpark::variant<int, std::string> local_variant_id_or_name;
//     double local_variant_score;
//     int local_variant_rank;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_variant_id_or_name",
//               "local_variant_score",
//               "local_variant_rank",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_variant_field_1) {
//   struct {
//     mpark::variant<bool, int> local_variant_flag_or_id;
//     std::string local_variant_label;
//     float local_variant_weight;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_variant_flag_or_id",
//               "local_variant_label",
//               "local_variant_weight",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_vector_field_2) {
//   struct {
//     int local_vector_key;
//     std::vector<double> local_vector_values;
//     std::string local_vector_note;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_vector_key",
//               "local_vector_values",
//               "local_vector_note",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_scalar_mix_3) {
//   struct {
//     unsigned long local_scalar_unsigned_long;
//     signed char local_scalar_signed_char;
//     double local_scalar_measurement;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_scalar_unsigned_long",
//               "local_scalar_signed_char",
//               "local_scalar_measurement",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_vector_field_3) {
//   struct {
//     const char *local_vector_source;
//     std::vector<char> local_vector_bytes;
//     int local_vector_size_hint;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_vector_source",
//               "local_vector_bytes",
//               "local_vector_size_hint",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_tuple_field_2) {
//   struct {
//     std::tuple<int, char, double> local_tuple_record;
//     std::string local_tuple_record_name;
//     bool local_tuple_record_ready;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_tuple_record",
//               "local_tuple_record_name",
//               "local_tuple_record_ready",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_variant_field_2) {
//   struct {
//     mpark::variant<int, double, std::string> local_variant_payload;
//     char local_variant_kind;
//     long local_variant_serial;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_variant_payload",
//               "local_variant_kind",
//               "local_variant_serial",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_nested_vector_tuple_field)
// {
//   struct {
//     std::vector<std::tuple<int, double>> local_nested_tuple_vector;
//     std::string local_nested_tuple_vector_name;
//     int local_nested_tuple_vector_count;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_nested_tuple_vector",
//               "local_nested_tuple_vector_name",
//               "local_nested_tuple_vector_count",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_nested_tuple_vector_field)
// {
//   struct {
//     std::tuple<std::vector<int>, int> local_nested_vector_tuple;
//     double local_nested_vector_tuple_ratio;
//     std::string local_nested_vector_tuple_note;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_nested_vector_tuple",
//               "local_nested_vector_tuple_ratio",
//               "local_nested_vector_tuple_note",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names,
//   in_cpp_local_unnamed_struct_with_nested_variant_vector_field) {
//   struct {
//     mpark::variant<std::vector<int>, int> local_nested_vector_variant;
//     std::string local_nested_vector_variant_label;
//     double local_nested_vector_variant_score;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_nested_vector_variant",
//               "local_nested_vector_variant_label",
//               "local_nested_vector_variant_score",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names,
//   in_cpp_local_unnamed_struct_with_nested_vector_variant_field) {
//   struct {
//     std::vector<mpark::variant<int, double>> local_nested_variant_vector;
//     int local_nested_variant_vector_index;
//     std::string local_nested_variant_vector_note;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_nested_variant_vector",
//               "local_nested_variant_vector_index",
//               "local_nested_variant_vector_note",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
TEST(print_names, in_cpp_unnamed_global) {
  ASSERT_EQ((std::vector<std::string>{
              "oceanic",
              "station",
              "bearing",
            }),
    omni::reflected_call(example_impl::print_field_names_simple,
      unnamed_global));
}

// FIXME: does not compile with Clang: named public bases are also
// emitted as indexed metadata, making _reflected<Base> ambiguous.
//
// TEST(print_names, in_cpp_unnamed_global_with_header_base) {
//   ASSERT_EQ((std::vector<std::string>{
//               "in_header_field_0",
//               "in_header_field_1",
//               "in_header_field_2",
//               "unnamed_global_header_base_field_0",
//               "unnamed_global_header_base_field_1",
//               "unnamed_global_header_base_field_2",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple,
//       unnamed_global_with_header_base));
// }
//
// TEST(print_names, in_cpp_unnamed_global_with_cpp_base) {
//   ASSERT_EQ((std::vector<std::string>{
//               "in_cpp_field_0",
//               "in_cpp_field_1",
//               "in_cpp_field_2",
//               "unnamed_global_cpp_base_field_0",
//               "unnamed_global_cpp_base_field_1",
//               "unnamed_global_cpp_base_field_2",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple,
//       unnamed_global_with_cpp_base));
// }
//
// TEST(print_names, in_cpp_unnamed_global_with_multi_base) {
//   ASSERT_EQ((std::vector<std::string>{
//               "in_header_field_0",
//               "in_header_field_1",
//               "in_header_field_2",
//               "in_cpp_field_0",
//               "in_cpp_field_1",
//               "in_cpp_field_2",
//               "unnamed_global_multi_base_field_0",
//               "unnamed_global_multi_base_field_1",
//               "unnamed_global_multi_base_field_2",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple,
//       unnamed_global_with_multi_base));
// }

// FIXME: does not compile with Clang: forward-declarable indexed records
// currently get both named and indexed _reflected specializations.
// Returned unnamed structs with named bases hit the same base ambiguity.
//
// TEST(print_names, in_header_struct) {
//   const example::in_header_struct p{};
//   const static std::vector<std::string> expected{
//     "in_header_field_0",
//     "in_header_field_1",
//     "in_header_field_2",
//   };
//   ASSERT_EQ(expected,
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// #if defined CXX_STANDARD && 11 < CXX_STANDARD
// TEST(print_names, in_cpp_unnamed_returned_struct) {
//   ASSERT_EQ((std::vector<std::string>{
//               "g_a",
//               "g_b",
//               "g_c",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple,
//       example::unnamed_returned_struct()));
// }
//
// TEST(print_names, in_cpp_unnamed_returned_struct_with_header_base) {
//   ASSERT_EQ((std::vector<std::string>{
//               "in_header_field_0",
//               "in_header_field_1",
//               "in_header_field_2",
//               "returned_header_base_field_0",
//               "returned_header_base_field_1",
//               "returned_header_base_field_2",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple,
//       example::unnamed_returned_struct_with_header_base()));
// }
//
// TEST(print_names, in_cpp_unnamed_returned_struct_with_cpp_base) {
//   ASSERT_EQ((std::vector<std::string>{
//               "in_cpp_field_0",
//               "in_cpp_field_1",
//               "in_cpp_field_2",
//               "returned_cpp_base_field_0",
//               "returned_cpp_base_field_1",
//               "returned_cpp_base_field_2",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple,
//       example::unnamed_returned_struct_with_cpp_base()));
// }
//
// TEST(print_names, in_cpp_unnamed_returned_struct_with_multi_base) {
//   ASSERT_EQ((std::vector<std::string>{
//               "in_header_field_0",
//               "in_header_field_1",
//               "in_header_field_2",
//               "in_cpp_field_0",
//               "in_cpp_field_1",
//               "in_cpp_field_2",
//               "returned_multi_base_field_0",
//               "returned_multi_base_field_1",
//               "returned_multi_base_field_2",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple,
//       example::unnamed_returned_struct_with_multi_base()));
// }
// #endif
//
// TEST(print_names, in_cpp_struct) {
//   const example::in_cpp_struct p{};
//   const static std::vector<std::string> expected{
//     "in_cpp_field_0",
//     "in_cpp_field_1",
//     "in_cpp_field_2",
//   };
//   ASSERT_EQ(expected,
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_derived_from_header_struct) {
//   const example::in_cpp_derived_from_header_struct p{};
//   const static std::vector<std::string> expected{
//     "in_header_field_0",
//     "in_header_field_1",
//     "in_header_field_2",
//     "in_cpp_derived_field_0",
//     "in_cpp_derived_field_1",
//     "in_cpp_derived_field_2",
//   };
//   ASSERT_EQ(expected,
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_derived_from_cpp_struct) {
//   const example::in_cpp_derived_from_cpp_struct p{};
//   const static std::vector<std::string> expected{
//     "in_cpp_field_0",
//     "in_cpp_field_1",
//     "in_cpp_field_2",
//     "in_cpp_derived_from_cpp_field_0",
//     "in_cpp_derived_from_cpp_field_1",
//     "in_cpp_derived_from_cpp_field_2",
//   };
//   ASSERT_EQ(expected,
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_multi_base) {
//   const example::in_cpp_multi_base p{};
//   const static std::vector<std::string> expected{
//     "in_header_field_0",
//     "in_header_field_1",
//     "in_header_field_2",
//     "in_cpp_field_0",
//     "in_cpp_field_1",
//     "in_cpp_field_2",
//     "in_cpp_multi_base_field_0",
//     "in_cpp_multi_base_field_1",
//     "in_cpp_multi_base_field_2",
//   };
//   ASSERT_EQ(expected,
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }

static const struct {
  template <typename Enum>
  std::vector<std::string> operator()(const Enum &) const noexcept {
    static_assert(omni::is_reflected<Enum>::value, "enum not reflected");
    const auto enums = omni::reflected_enum_t<Enum>::enumerators();
    std::vector<std::string> names;
    for (const auto &value_name : enums)
      names.emplace_back(value_name.second);

    return names;
  }
} get_enumerators{};

// FIXME: does not compile: generated forward declaration is emitted as
// `enum in_cpp_enum;`, but unscoped enum forward declarations require a fixed
// underlying type.
//
// TEST(print_enums, in_cpp_enum) {
//   const example::in_cpp_enum e{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "in_cpp_enum_a",
//               "in_cpp_enum_b",
//               "in_cpp_enum_c",
//             }),
//     omni::reflected_call(get_enumerators, e));
// }

TEST(print_enums, in_cpp_unnamed_global_enum) {
  ASSERT_EQ((std::vector<std::string>{
              "unnamed_global_enum_a",
              "unnamed_global_enum_b",
              "unnamed_global_enum_c",
            }),
    omni::reflected_call(get_enumerators, unnamed_global_enum));
}

// FIXME: does not compile with Clang: forward-declarable indexed enums
// currently get both named and indexed _reflected specializations.
//
// TEST(print_enums, in_cpp_scoped_enum) {
//   const example::in_cpp_scoped_enum e{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "in_cpp_scoped_enum_a",
//               "in_cpp_scoped_enum_b",
//               "in_cpp_scoped_enum_c",
//             }),
//     omni::reflected_call(get_enumerators, e));
// }

// FIXME: does not compile: generated enum forward declaration loses the
// namespace and fixed underlying type.
//
// TEST(print_enums, in_cpp_fixed_enum) {
//   const example::in_cpp_fixed_enum e{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "in_cpp_fixed_enum_a",
//               "in_cpp_fixed_enum_b",
//               "in_cpp_fixed_enum_c",
//             }),
//     omni::reflected_call(get_enumerators, e));
// }

// FIXME: does not compile: scoped enum forward declaration is generated without
// the explicit underlying type.
//
// TEST(print_enums, in_cpp_scoped_enum_with_underlying) {
//   const example::in_cpp_scoped_enum_with_underlying e{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "in_cpp_scoped_enum_with_underlying_a",
//               "in_cpp_scoped_enum_with_underlying_b",
//               "in_cpp_scoped_enum_with_underlying_c",
//             }),
//     omni::reflected_call(get_enumerators, e));
// }

TEST(print_enums, in_cpp_local_unnamed_enum) {
  enum {
    unnamed_a,
    unnamed_b,
  } const e{};

  static const std::vector<std::string> k_expected{
    "unnamed_a",
    "unnamed_b",
  };
  ASSERT_EQ(k_expected, omni::reflected_call(get_enumerators, e));

#if defined CXX_STANDARD && 11 < CXX_STANDARD
  ASSERT_EQ(k_expected,
    omni::reflected_call(
      // Inplace template lambdas are also possible (starting from C++14).
      // IMPORTANT NOTE: Trailing return type _must_ be specified, otherwise AST
      // parser will go inside the body to evaluate the return type, thus
      // breaking the tool run.
      [](const auto &v) -> std::vector<std::string> {
        using Enum = decltype(v);

        static_assert(omni::is_reflected<Enum>::value, "enum not reflected");
        const auto enums = omni::reflected_enum_t<Enum>::enumerators();
        std::vector<std::string> names;
        for (const auto &value_name : enums)
          names.emplace_back(value_name.second);

        return names;
      },
      e));

#endif
}

// FIXME: does not generate: reflecting types with private/protected bases or
// non-public fields currently fails header generation. Expected behavior is
// to reflect only own public fields and public-base fields.

TEST(print_names, in_cpp_private_base) {
  const example::in_cpp_private_base p{};
  const static std::vector<std::string> expected{
    "in_cpp_private_base_field_0",
    "in_cpp_private_base_field_1",
    "in_cpp_private_base_field_2",
  };
  ASSERT_EQ(expected,
    omni::reflected_call(example_impl::print_field_names_simple, p));
}

TEST(print_names, in_cpp_protected_base) {
  const example::in_cpp_protected_base p{};
  const static std::vector<std::string> expected{
    "in_cpp_protected_base_field_0",
    "in_cpp_protected_base_field_1",
    "in_cpp_protected_base_field_2",
  };
  ASSERT_EQ(expected,
    omni::reflected_call(example_impl::print_field_names_simple, p));
}

TEST(print_names, in_cpp_mixed_access) {
  const example::in_cpp_mixed_access p{};
  const static std::vector<std::string> expected{
    "in_cpp_public_field_0",
    "in_cpp_public_field_1",
    "in_cpp_public_field_2",
  };
  ASSERT_EQ(expected,
    omni::reflected_call(example_impl::print_field_names_simple, p));
}

// FIXME: does not compile: the dependency base is generated both as a named
// specialization and as an indexed specialization, making _reflected ambiguous.
//
// TEST(print_names, in_cpp_deep_public_base_chain) {
//   const example::in_cpp_deep_derived p{};
//   const static std::vector<std::string> expected{
//     "in_header_field_0",
//     "in_header_field_1",
//     "in_header_field_2",
//     "in_cpp_mid_field_0",
//     "in_cpp_mid_field_1",
//     "in_cpp_mid_field_2",
//     "in_cpp_deep_field_0",
//     "in_cpp_deep_field_1",
//     "in_cpp_deep_field_2",
//   };
//   ASSERT_EQ(expected,
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }

// FIXME: does not compile: unnamed global types with deep public base chains
// hit the same ambiguous _reflected specialization as named deep base chains.
//
// TEST(print_names, in_cpp_unnamed_global_with_deep_public_base_chain) {
//   struct: example::in_cpp_mid_base {
//     std::string unnamed_global_deep_base_field_0;
//     int unnamed_global_deep_base_field_1;
//     double unnamed_global_deep_base_field_2;
//   } const p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "in_header_field_0",
//               "in_header_field_1",
//               "in_header_field_2",
//               "in_cpp_mid_field_0",
//               "in_cpp_mid_field_1",
//               "in_cpp_mid_field_2",
//               "unnamed_global_deep_base_field_0",
//               "unnamed_global_deep_base_field_1",
//               "unnamed_global_deep_base_field_2",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }

// FIXME: does not compile: returned unnamed types with deep public base chains
// hit the same ambiguous _reflected specialization as named deep base chains.
//
// TEST(print_names, in_cpp_unnamed_returned_struct_with_deep_public_base_chain)
// {
//   struct make {
//     auto operator()() const {
//       struct: example::in_cpp_mid_base {
//         std::string returned_deep_base_field_0;
//         int returned_deep_base_field_1;
//         double returned_deep_base_field_2;
//       } p{};
//       return p;
//     }
//   };
//
//   ASSERT_EQ((std::vector<std::string>{
//               "in_header_field_0",
//               "in_header_field_1",
//               "in_header_field_2",
//               "in_cpp_mid_field_0",
//               "in_cpp_mid_field_1",
//               "in_cpp_mid_field_2",
//               "returned_deep_base_field_0",
//               "returned_deep_base_field_1",
//               "returned_deep_base_field_2",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, make{}()));
// }

// FIXME: does not compile: local named class specializations are rendered
// through the GTest fixture class before that class is declared in the forced
// include.
//
// TEST(print_names, in_cpp_local_named_struct) {
//   struct local_named_struct {
//     std::string in_cpp_local_named_field_0;
//     int in_cpp_local_named_field_1;
//     double in_cpp_local_named_field_2;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "in_cpp_local_named_field_0",
//               "in_cpp_local_named_field_1",
//               "in_cpp_local_named_field_2",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }

// FIXME: does not compile: local named class specializations are rendered
// through the GTest fixture class before that class is declared in the forced
// include.
//
// TEST(print_names, in_cpp_local_named_struct_with_base) {
//   struct local_named_struct: example::in_cpp_struct {
//     std::string in_cpp_local_named_field_0;
//     int in_cpp_local_named_field_1;
//     double in_cpp_local_named_field_2;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "in_cpp_field_0",
//               "in_cpp_field_1",
//               "in_cpp_field_2",
//               "in_cpp_local_named_field_0",
//               "in_cpp_local_named_field_1",
//               "in_cpp_local_named_field_2",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }

// FIXME: does not compile: local base and derived types need direct indexed
// specializations, but the current generated header tries to route them through
// the not-yet-declared GTest fixture class.
//
// TEST(print_names, in_cpp_local_public_base_chain) {
//   struct local_base {
//     std::string in_cpp_local_base_field_0;
//     int in_cpp_local_base_field_1;
//     double in_cpp_local_base_field_2;
//   };
//
//   struct local_derived: local_base {
//     std::string in_cpp_local_derived_field_0;
//     int in_cpp_local_derived_field_1;
//     double in_cpp_local_derived_field_2;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "in_cpp_local_base_field_0",
//               "in_cpp_local_base_field_1",
//               "in_cpp_local_base_field_2",
//               "in_cpp_local_derived_field_0",
//               "in_cpp_local_derived_field_1",
//               "in_cpp_local_derived_field_2",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }

// FIXME: does not generate: reflecting local unnamed structs with private bases
// currently fails header generation. Expected behavior is to reflect only own
// public fields.

TEST(print_names, in_cpp_local_unnamed_struct_with_private_base) {
  struct: private example::in_cpp_struct {
    std::string in_cpp_local_private_base_field_0;
    int in_cpp_local_private_base_field_1;
    double in_cpp_local_private_base_field_2;
  } p{};

  ASSERT_EQ((std::vector<std::string>{
              "in_cpp_local_private_base_field_0",
              "in_cpp_local_private_base_field_1",
              "in_cpp_local_private_base_field_2",
            }),
    omni::reflected_call(example_impl::print_field_names_simple, p));
}

// TODO: support dependency types that cannot be forward-declared. Some of
// these can be generated via decltype(member field); incidental indexes from
// unrelated reflected_call sites should not affect support.
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_unscoped_enum_field) {
//   enum local_field_enum {
//     local_field_enum_a,
//     local_field_enum_b,
//   };
//
//   struct {
//     local_field_enum in_cpp_local_enum_field_0;
//     example::in_cpp_enum in_cpp_local_enum_field_1;
//     int in_cpp_local_enum_field_2;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "in_cpp_local_enum_field_0",
//               "in_cpp_local_enum_field_1",
//               "in_cpp_local_enum_field_2",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_scoped_enum_field) {
//   enum class local_field_enum_class {
//     local_field_enum_class_a,
//     local_field_enum_class_b,
//   };
//
//   struct {
//     local_field_enum_class in_cpp_local_enum_class_field_0;
//     example::in_cpp_scoped_enum in_cpp_local_enum_class_field_1;
//     int in_cpp_local_enum_class_field_2;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "in_cpp_local_enum_class_field_0",
//               "in_cpp_local_enum_class_field_1",
//               "in_cpp_local_enum_class_field_2",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_unnamed_field) {
//   struct {
//     decltype(unnamed_global) in_cpp_local_unnamed_dependency_field_0;
//     std::string in_cpp_local_unnamed_dependency_field_1;
//     int in_cpp_local_unnamed_dependency_field_2;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "in_cpp_local_unnamed_dependency_field_0",
//               "in_cpp_local_unnamed_dependency_field_1",
//               "in_cpp_local_unnamed_dependency_field_2",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_local_unnamed_field) {
//   struct {
//     std::string local_unnamed_dependency_field_0;
//     int local_unnamed_dependency_field_1;
//     double local_unnamed_dependency_field_2;
//   } local_unnamed_dependency{};
//
//   struct {
//     decltype(local_unnamed_dependency) in_cpp_local_local_dependency_field_0;
//     std::string in_cpp_local_local_dependency_field_1;
//     int in_cpp_local_local_dependency_field_2;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "in_cpp_local_local_dependency_field_0",
//               "in_cpp_local_local_dependency_field_1",
//               "in_cpp_local_local_dependency_field_2",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_unnamed_base) {
//   struct: decltype(unnamed_global) {
//     std::string in_cpp_local_unnamed_base_field_0;
//     int in_cpp_local_unnamed_base_field_1;
//     double in_cpp_local_unnamed_base_field_2;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "oceanic",
//               "station",
//               "bearing",
//               "in_cpp_local_unnamed_base_field_0",
//               "in_cpp_local_unnamed_base_field_1",
//               "in_cpp_local_unnamed_base_field_2",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }

// FIXME: does not generate: reflecting local unnamed structs with non-public
// fields currently fails header generation. Expected behavior is to reflect
// only public fields.

TEST(print_names, in_cpp_local_unnamed_struct_with_mixed_access) {
  struct {
    std::string in_cpp_local_public_field_0;
    int in_cpp_local_public_field_1;

    private:
    std::string in_cpp_local_private_field;

    public:
    double in_cpp_local_public_field_2;

    protected:
    int in_cpp_local_protected_field;
  } p{};

  ASSERT_EQ((std::vector<std::string>{
              "in_cpp_local_public_field_0",
              "in_cpp_local_public_field_1",
              "in_cpp_local_public_field_2",
            }),
    omni::reflected_call(example_impl::print_field_names_simple, p));
}

// FIXME: does not generate: the unnamed type used through std::vector is
// collected as a dependency, but header mode reports non-reflectable types.
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_unnamed_vector_field) {
//   struct {
//     std::string unnamed_template_arg_field_0;
//     int unnamed_template_arg_field_1;
//     double unnamed_template_arg_field_2;
//   } unnamed_template_arg{};
//
//   struct {
//     std::vector<decltype(unnamed_template_arg)> in_cpp_route_vector;
//     std::string in_cpp_route_vector_owner_field_1;
//     int in_cpp_route_vector_owner_field_2;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "in_cpp_route_vector",
//               "in_cpp_route_vector_owner_field_1",
//               "in_cpp_route_vector_owner_field_2",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }

// FIXME: does not generate: the unnamed type used through std::tuple is
// collected as a dependency, but header mode reports non-reflectable types.
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_unnamed_tuple_field) {
//   struct {
//     std::string unnamed_template_arg_field_0;
//     int unnamed_template_arg_field_1;
//     double unnamed_template_arg_field_2;
//   } unnamed_template_arg{};
//
//   struct {
//     std::tuple<decltype(unnamed_template_arg), int> in_cpp_route_tuple;
//     std::string in_cpp_route_tuple_owner_field_1;
//     int in_cpp_route_tuple_owner_field_2;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "in_cpp_route_tuple",
//               "in_cpp_route_tuple_owner_field_1",
//               "in_cpp_route_tuple_owner_field_2",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }

// FIXME: does not generate: the unnamed type used through mpark::variant is
// collected as a dependency, but header mode reports non-reflectable types.
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_unnamed_variant_field) {
//   struct {
//     std::string unnamed_template_arg_field_0;
//     int unnamed_template_arg_field_1;
//     double unnamed_template_arg_field_2;
//   } unnamed_template_arg{};
//
//   struct {
//     mpark::variant<decltype(unnamed_template_arg), int> in_cpp_route_variant;
//     std::string in_cpp_route_variant_owner_field_1;
//     int in_cpp_route_variant_owner_field_2;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "in_cpp_route_variant",
//               "in_cpp_route_variant_owner_field_1",
//               "in_cpp_route_variant_owner_field_2",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }

// TODO: support dependency types that cannot be forward-declared through
// sequence templates. Some of these can be generated via decltype(member field).
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_enum_vector_field) {
//   enum local_vector_enum {
//     local_vector_enum_a,
//     local_vector_enum_b,
//   };
//
//   struct {
//     std::vector<local_vector_enum> in_cpp_enum_vector_field_0;
//     std::string in_cpp_enum_vector_field_1;
//     int in_cpp_enum_vector_field_2;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "in_cpp_enum_vector_field_0",
//               "in_cpp_enum_vector_field_1",
//               "in_cpp_enum_vector_field_2",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_enum_tuple_field) {
//   enum local_tuple_enum {
//     local_tuple_enum_a,
//     local_tuple_enum_b,
//   };
//
//   struct {
//     std::tuple<local_tuple_enum, int> in_cpp_enum_tuple_field_0;
//     std::string in_cpp_enum_tuple_field_1;
//     int in_cpp_enum_tuple_field_2;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "in_cpp_enum_tuple_field_0",
//               "in_cpp_enum_tuple_field_1",
//               "in_cpp_enum_tuple_field_2",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_enum_variant_field) {
//   enum local_variant_enum {
//     local_variant_enum_a,
//     local_variant_enum_b,
//   };
//
//   struct {
//     mpark::variant<local_variant_enum, int> in_cpp_enum_variant_field_0;
//     std::string in_cpp_enum_variant_field_1;
//     int in_cpp_enum_variant_field_2;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "in_cpp_enum_variant_field_0",
//               "in_cpp_enum_variant_field_1",
//               "in_cpp_enum_variant_field_2",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }

// FIXME: does not compile: local scoped enum specializations are rendered
// through the GTest fixture class before that class is declared in the forced
// include.
//
// TEST(print_enums, in_cpp_local_scoped_enum) {
//   enum class local_scoped_enum {
//     scoped_a,
//     scoped_b,
//     scoped_c,
//   };
//   const local_scoped_enum e{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "scoped_a",
//               "scoped_b",
//               "scoped_c",
//             }),
//     omni::reflected_call(get_enumerators, e));
// }

// FIXME: does not compile: local named class declarations are emitted through
// the GTest fixture scope before that scope can be named from the forced
// include. These cases describe additional local/context/template-route
// combinations that should eventually be generated through indexed
// specializations.
//
// TEST(print_names, in_cpp_local_named_struct_with_std_vector_field) {
//   struct local_named_struct {
//     std::vector<int> local_named_vector_values;
//     std::string local_named_vector_label;
//     double local_named_vector_score;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_named_vector_values",
//               "local_named_vector_label",
//               "local_named_vector_score",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_named_struct_with_tuple_field) {
//   struct local_named_struct {
//     std::tuple<int, double> local_named_tuple_pair;
//     std::string local_named_tuple_label;
//     int local_named_tuple_count;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_named_tuple_pair",
//               "local_named_tuple_label",
//               "local_named_tuple_count",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_named_struct_with_variant_field) {
//   struct local_named_struct {
//     mpark::variant<int, std::string> local_named_variant_payload;
//     double local_named_variant_score;
//     int local_named_variant_rank;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_named_variant_payload",
//               "local_named_variant_score",
//               "local_named_variant_rank",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_named_struct_with_header_base_and_vector) {
//   struct local_named_struct: example::in_header_struct {
//     std::vector<int> local_named_header_vector_values;
//     std::string local_named_header_vector_label;
//     double local_named_header_vector_score;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "in_header_field_0",
//               "in_header_field_1",
//               "in_header_field_2",
//               "local_named_header_vector_values",
//               "local_named_header_vector_label",
//               "local_named_header_vector_score",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_named_struct_with_cpp_base_and_tuple) {
//   struct local_named_struct: example::in_cpp_struct {
//     std::tuple<int, double> local_named_cpp_tuple_pair;
//     std::string local_named_cpp_tuple_label;
//     int local_named_cpp_tuple_count;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "in_cpp_field_0",
//               "in_cpp_field_1",
//               "in_cpp_field_2",
//               "local_named_cpp_tuple_pair",
//               "local_named_cpp_tuple_label",
//               "local_named_cpp_tuple_count",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_named_struct_with_multi_base_and_variant) {
//   struct local_named_struct: example::in_header_struct,
//   example::in_cpp_struct {
//     mpark::variant<int, std::string> local_named_multi_variant_payload;
//     double local_named_multi_variant_score;
//     int local_named_multi_variant_rank;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "in_header_field_0",
//               "in_header_field_1",
//               "in_header_field_2",
//               "in_cpp_field_0",
//               "in_cpp_field_1",
//               "in_cpp_field_2",
//               "local_named_multi_variant_payload",
//               "local_named_multi_variant_score",
//               "local_named_multi_variant_rank",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_named_struct_with_private_base_and_vector) {
//   struct local_named_struct: private example::in_cpp_struct {
//     std::vector<int> local_named_private_vector_values;
//     std::string local_named_private_vector_label;
//     double local_named_private_vector_score;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_named_private_vector_values",
//               "local_named_private_vector_label",
//               "local_named_private_vector_score",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_named_struct_with_protected_base_and_tuple) {
//   struct local_named_struct: protected example::in_header_struct {
//     std::tuple<int, double> local_named_protected_tuple_pair;
//     std::string local_named_protected_tuple_label;
//     int local_named_protected_tuple_count;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_named_protected_tuple_pair",
//               "local_named_protected_tuple_label",
//               "local_named_protected_tuple_count",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_named_struct_with_non_public_fields) {
//   class local_named_struct {
//     std::string local_named_implicit_private_field;
//
//   public:
//     std::string local_named_public_field_0;
//     int local_named_public_field_1;
//     double local_named_public_field_2;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_named_public_field_0",
//               "local_named_public_field_1",
//               "local_named_public_field_2",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_named_struct_with_local_enum_field) {
//   enum local_enum {
//     local_enum_a,
//     local_enum_b,
//     local_enum_c,
//   };
//
//   struct local_named_struct {
//     local_enum local_named_enum_field;
//     std::string local_named_enum_label;
//     int local_named_enum_rank;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_named_enum_field",
//               "local_named_enum_label",
//               "local_named_enum_rank",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_named_struct_with_local_scoped_enum_field) {
//   enum class local_scoped_enum {
//     local_scoped_enum_a,
//     local_scoped_enum_b,
//   };
//
//   struct local_named_struct {
//     local_scoped_enum local_named_scoped_enum_field;
//     std::string local_named_scoped_enum_label;
//     int local_named_scoped_enum_rank;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_named_scoped_enum_field",
//               "local_named_scoped_enum_label",
//               "local_named_scoped_enum_rank",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_local_named_field) {
//   struct local_named_dependency {
//     std::string local_named_dependency_field_0;
//     int local_named_dependency_field_1;
//     double local_named_dependency_field_2;
//   };
//
//   struct {
//     local_named_dependency local_named_dependency_value;
//     std::string local_named_dependency_label;
//     int local_named_dependency_rank;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_named_dependency_value",
//               "local_named_dependency_label",
//               "local_named_dependency_rank",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_local_named_vector_field)
// {
//   struct local_named_dependency {
//     std::string local_named_vector_dependency_field_0;
//     int local_named_vector_dependency_field_1;
//     double local_named_vector_dependency_field_2;
//   };
//
//   struct {
//     std::vector<local_named_dependency> local_named_dependency_vector;
//     std::string local_named_dependency_vector_label;
//     int local_named_dependency_vector_rank;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_named_dependency_vector",
//               "local_named_dependency_vector_label",
//               "local_named_dependency_vector_rank",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_local_named_tuple_field) {
//   struct local_named_dependency {
//     std::string local_named_tuple_dependency_field_0;
//     int local_named_tuple_dependency_field_1;
//     double local_named_tuple_dependency_field_2;
//   };
//
//   struct {
//     std::tuple<local_named_dependency, int> local_named_dependency_tuple;
//     std::string local_named_dependency_tuple_label;
//     int local_named_dependency_tuple_rank;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_named_dependency_tuple",
//               "local_named_dependency_tuple_label",
//               "local_named_dependency_tuple_rank",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_local_named_variant_field)
// {
//   struct local_named_dependency {
//     std::string local_named_variant_dependency_field_0;
//     int local_named_variant_dependency_field_1;
//     double local_named_variant_dependency_field_2;
//   };
//
//   struct {
//     mpark::variant<local_named_dependency, int>
//     local_named_dependency_variant; std::string
//     local_named_dependency_variant_label; int
//     local_named_dependency_variant_rank;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_named_dependency_variant",
//               "local_named_dependency_variant_label",
//               "local_named_dependency_variant_rank",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names,
// in_cpp_local_unnamed_struct_with_local_unnamed_vector_nested)
// {
//   struct {
//     std::string local_unnamed_vector_dependency_field_0;
//     int local_unnamed_vector_dependency_field_1;
//     double local_unnamed_vector_dependency_field_2;
//   } local_unnamed_dependency{};
//
//   struct {
//     std::vector<std::vector<decltype(local_unnamed_dependency)>>
//       local_unnamed_dependency_nested_vector;
//     std::string local_unnamed_dependency_nested_vector_label;
//     int local_unnamed_dependency_nested_vector_rank;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_unnamed_dependency_nested_vector",
//               "local_unnamed_dependency_nested_vector_label",
//               "local_unnamed_dependency_nested_vector_rank",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names,
// in_cpp_local_unnamed_struct_with_local_unnamed_tuple_nested)
// {
//   struct {
//     std::string local_unnamed_tuple_dependency_field_0;
//     int local_unnamed_tuple_dependency_field_1;
//     double local_unnamed_tuple_dependency_field_2;
//   } local_unnamed_dependency{};
//
//   struct {
//     std::tuple<std::tuple<decltype(local_unnamed_dependency), int>, double>
//       local_unnamed_dependency_nested_tuple;
//     std::string local_unnamed_dependency_nested_tuple_label;
//     int local_unnamed_dependency_nested_tuple_rank;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_unnamed_dependency_nested_tuple",
//               "local_unnamed_dependency_nested_tuple_label",
//               "local_unnamed_dependency_nested_tuple_rank",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names,
// in_cpp_local_unnamed_struct_with_local_unnamed_variant_nested)
// {
//   struct {
//     std::string local_unnamed_variant_dependency_field_0;
//     int local_unnamed_variant_dependency_field_1;
//     double local_unnamed_variant_dependency_field_2;
//   } local_unnamed_dependency{};
//
//   struct {
//     mpark::variant<mpark::variant<decltype(local_unnamed_dependency), int>,
//       double>
//       local_unnamed_dependency_nested_variant;
//     std::string local_unnamed_dependency_nested_variant_label;
//     int local_unnamed_dependency_nested_variant_rank;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_unnamed_dependency_nested_variant",
//               "local_unnamed_dependency_nested_variant_label",
//               "local_unnamed_dependency_nested_variant_rank",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_decltype_global_vector) {
//   struct {
//     std::vector<decltype(unnamed_global)> global_unnamed_vector_dependency;
//     std::string global_unnamed_vector_dependency_label;
//     int global_unnamed_vector_dependency_rank;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "global_unnamed_vector_dependency",
//               "global_unnamed_vector_dependency_label",
//               "global_unnamed_vector_dependency_rank",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_decltype_global_tuple) {
//   struct {
//     std::tuple<decltype(unnamed_global), int>
//     global_unnamed_tuple_dependency; std::string
//     global_unnamed_tuple_dependency_label; int
//     global_unnamed_tuple_dependency_rank;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "global_unnamed_tuple_dependency",
//               "global_unnamed_tuple_dependency_label",
//               "global_unnamed_tuple_dependency_rank",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_decltype_global_variant) {
//   struct {
//     mpark::variant<decltype(unnamed_global), int>
//       global_unnamed_variant_dependency;
//     std::string global_unnamed_variant_dependency_label;
//     int global_unnamed_variant_dependency_rank;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "global_unnamed_variant_dependency",
//               "global_unnamed_variant_dependency_label",
//               "global_unnamed_variant_dependency_rank",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_private_base_and_vector) {
//   struct: private example::in_cpp_struct {
//     std::vector<int> private_base_vector_values;
//     std::string private_base_vector_label;
//     double private_base_vector_score;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "private_base_vector_values",
//               "private_base_vector_label",
//               "private_base_vector_score",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_protected_base_and_tuple)
// {
//   struct: protected example::in_header_struct {
//     std::tuple<int, double> protected_base_tuple_pair;
//     std::string protected_base_tuple_label;
//     int protected_base_tuple_count;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "protected_base_tuple_pair",
//               "protected_base_tuple_label",
//               "protected_base_tuple_count",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_names, in_cpp_local_unnamed_struct_with_mixed_access_and_variant)
// {
//   class local_unnamed_like {
//     mpark::variant<int, std::string> mixed_access_private_payload;
//
//   public:
//     std::string mixed_access_public_label;
//     int mixed_access_public_rank;
//     double mixed_access_public_score;
//   } p{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "mixed_access_public_label",
//               "mixed_access_public_rank",
//               "mixed_access_public_score",
//             }),
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
//
// TEST(print_enums, in_cpp_local_named_fixed_enum) {
//   enum local_fixed_enum : int {
//     local_fixed_enum_a,
//     local_fixed_enum_b,
//     local_fixed_enum_c,
//   };
//   const local_fixed_enum e{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_fixed_enum_a",
//               "local_fixed_enum_b",
//               "local_fixed_enum_c",
//             }),
//     omni::reflected_call(get_enumerators, e));
// }
//
// TEST(print_enums, in_cpp_local_scoped_enum_with_underlying) {
//   enum class local_scoped_enum : unsigned {
//     local_scoped_enum_a,
//     local_scoped_enum_b,
//     local_scoped_enum_c,
//   };
//   const local_scoped_enum e{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_scoped_enum_a",
//               "local_scoped_enum_b",
//               "local_scoped_enum_c",
//             }),
//     omni::reflected_call(get_enumerators, e));
// }
//
// TEST(print_enums, in_cpp_local_unnamed_fixed_enum) {
//   enum : int {
//     local_unnamed_fixed_enum_a,
//     local_unnamed_fixed_enum_b,
//     local_unnamed_fixed_enum_c,
//   } const e{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_unnamed_fixed_enum_a",
//               "local_unnamed_fixed_enum_b",
//               "local_unnamed_fixed_enum_c",
//             }),
//     omni::reflected_call(get_enumerators, e));
// }
//
// TEST(print_enums, in_cpp_local_unnamed_enum_three_values) {
//   enum {
//     local_unnamed_enum_a,
//     local_unnamed_enum_b,
//     local_unnamed_enum_c,
//   } const e{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_unnamed_enum_a",
//               "local_unnamed_enum_b",
//               "local_unnamed_enum_c",
//             }),
//     omni::reflected_call(get_enumerators, e));
// }
//
// TEST(print_enums, in_cpp_local_unnamed_enum_with_values) {
//   enum {
//     local_unnamed_enum_value_a = 4,
//     local_unnamed_enum_value_b = 8,
//     local_unnamed_enum_value_c = 15,
//   } const e{};
//
//   ASSERT_EQ((std::vector<std::string>{
//               "local_unnamed_enum_value_a",
//               "local_unnamed_enum_value_b",
//               "local_unnamed_enum_value_c",
//             }),
//     omni::reflected_call(get_enumerators, e));
// }
//
/////// Templates

namespace example {
template <typename T>
struct in_cpp_template;

template <>
struct in_cpp_template<int>: in_header_struct {
  std::string in_cpp_template_field_0;
  int in_cpp_template_field_1;
  double in_cpp_template_field_2;
};
} // namespace example

// FIXME: does not compile: generated forward declaration is emitted as
// `struct in_cpp_template<int>;` instead of preserving the class-template form.
//
// TEST(print_names, in_cpp_template_specialization_with_base) {
//   const example::in_cpp_template<int> p{};
//   const static std::vector<std::string> expected{
//     "in_header_field_0",
//     "in_header_field_1",
//     "in_header_field_2",
//     "in_cpp_template_field_0",
//     "in_cpp_template_field_1",
//     "in_cpp_template_field_2",
//   };
//   ASSERT_EQ(expected,
//     omni::reflected_call(example_impl::print_field_names_simple, p));
// }
