
#include "gtest_include.h"
#include "inplace_structs.h"

#include <omnirefl/reflected_call.hpp>
#include <omnirefl/reflected_scope.hpp>

#include <mpark/variant.hpp>

#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#if defined CXX_STANDARD && 17 <= CXX_STANDARD
#  include <variant>
#endif
#include <vector>

// This file is intentionally large: header-mode in-place reflection relies on
// stable indexed type ordering, and broad coverage here helps detect valid
// indexing-order regressions.

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

// TODO: move shared test variant helpers into a separate test header.
namespace compat {
template <typename T, typename... V>
T *get_if(mpark::variant<V...> *value) {
  return mpark::get_if<T>(value);
}

template <typename T, typename... V>
const T *get_if(const mpark::variant<V...> *value) {
  return mpark::get_if<T>(value);
}

#if defined CXX_STANDARD && 17 <= CXX_STANDARD
template <typename T, typename... V>
T *get_if(std::variant<V...> *value) {
  return std::get_if<T>(value);
}

template <typename T, typename... V>
const T *get_if(const std::variant<V...> *value) {
  return std::get_if<T>(value);
}
#endif
} // namespace compat

namespace example_impl {
template <typename T, typename V>
void from_std_map(const std::map<std::string, V> &from, T &to);

template <typename T, typename V>
T from_std_map(const std::map<std::string, V> &from);

// FIXME: leaking reflected-scope query helper, but not the index mismatch
// cause unless instantiated outside a reflected scope.
template <typename T, bool = omni::is_reflected<T>::value>
struct is_reflected_record: std::false_type {};

template <typename T>
struct is_reflected_record<T, true>:
    std::integral_constant<bool,
      omni::reflected_entity::record == omni::reflected_t<T>::entity()> {};

template <typename T>
struct is_string_map: std::false_type {};

template <typename V, typename C, typename A>
struct is_string_map<std::map<std::string, V, C, A>>: std::true_type {};

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

struct print_tuple_first_field_names_t {
  template <typename Tuple>
  std::vector<std::string> operator()(const Tuple &) const {
    using first_type = typename std::tuple_element<0,
      typename std::decay<Tuple>::type>::type;
    const auto fields = omni::reflected<first_type>().public_fields();
    return omni::compat::apply(print_field_names_simple_t::_visit{}, fields);
  }
} const static print_tuple_first_field_names{};

struct print_tuple_second_field_names_t {
  template <typename Tuple>
  std::vector<std::string> operator()(const Tuple &) const {
    using second_type = typename std::tuple_element<1,
      typename std::decay<Tuple>::type>::type;
    const auto fields = omni::reflected<second_type>().public_fields();
    return omni::compat::apply(print_field_names_simple_t::_visit{}, fields);
  }
} const static print_tuple_second_field_names{};

struct maybe_print_field_names_t {
  std::vector<std::string> operator()(int) const {
    return {};
  }

  std::vector<std::string> operator()(const std::string &) const {
    return {};
  }

  template <typename T>
  std::vector<std::string> operator()(const T &t) const {
    static_assert(omni::is_reflected<T>::value, "");
    return print_field_names_simple(t);
  }
} const static maybe_print_field_names{};

struct field_values_simple_t {
  template <typename T>
  std::vector<std::string> operator()(const T &t) const {
    const auto fields = omni::reflected(t).public_fields();
    return omni::compat::apply(*this, fields);
  }

  template <typename... Field>
  std::vector<std::string> operator()(const Field &...field) const {
    std::vector<std::string> out;
    int dummy[] = {0, (_append(out, field), 0)...};
    (void)dummy;
    return out;
  }

  private:
  template <typename Field>
  static typename std::enable_if<
    std::is_same<std::string, typename Field::type>::value>::type
    _append(std::vector<std::string> &out, const Field &field) {
    out.emplace_back(field.value());
  }

  template <typename Field>
  static typename std::enable_if<
    std::is_same<int, typename Field::type>::value>::type
    _append(std::vector<std::string> &out, const Field &field) {
    out.emplace_back(std::to_string(field.value()));
  }

  template <typename Field>
  static typename std::enable_if<
    std::is_same<unsigned, typename Field::type>::value>::type
    _append(std::vector<std::string> &out, const Field &field) {
    out.emplace_back(std::to_string(field.value()));
  }

  template <typename Field>
  static typename std::enable_if<
    std::is_same<double, typename Field::type>::value>::type
    _append(std::vector<std::string> &out, const Field &field) {
    std::ostringstream s;
    s << field.value();
    out.emplace_back(s.str());
  }

  template <typename Field>
  static typename std::enable_if<
    !std::is_same<std::string, typename Field::type>::value
      && !std::is_same<int, typename Field::type>::value
      && !std::is_same<unsigned, typename Field::type>::value
      && !std::is_same<double, typename Field::type>::value>::type
    _append(std::vector<std::string> &, const Field &) {}
} const static field_values_simple{};

struct write_fields_from_std_map {
  template <typename V, typename... Field>
  void operator()(const std::map<std::string, V> &from, Field... field) const {
    int dummy[] = {0, (_write_field(from, field), 0)...};
    (void)dummy;
  }

  private:
  template <typename V, typename Field>
  static typename std::enable_if<
    !is_reflected_record<typename Field::type>::value>::type
    _write_field(const std::map<std::string, V> &from, Field field) {
    if (0 == from.count(field.name()))
      return;

    const auto *value =
      compat::get_if<typename Field::type>(&from.at(field.name()));
    if (!value)
      return;

    // TODO: use the frontend mutability interface once it exists.
    field.set_value(*value);
  }

  template <typename V, typename Field>
  static typename std::enable_if<
    is_reflected_record<typename Field::type>::value>::type
    _write_field(const std::map<std::string, V> &from, Field field) {
    if (0 != from.count(field.name())
        && _write_field_from_nested_map(from.at(field.name()), field))
      return;

    // TODO: use the frontend mutability interface once it exists.
    field.set_value(from_std_map<typename Field::type>(from));
  }

  template <typename Field, typename Nested>
  static typename std::enable_if<is_string_map<Nested>::value, bool>::type
    _try_nested_map(const Nested *nested, Field field) {
    if (!nested)
      return false;

    // TODO: use the frontend mutability interface once it exists.
    field.set_value(from_std_map<typename Field::type>(*nested));
    return true;
  }

  template <typename Field, typename T>
  static typename std::enable_if<!is_string_map<T>::value, bool>::type
    _try_nested_map(const T *, Field) {
    return false;
  }

  template <typename Field, typename... V>
  static bool _write_field_from_nested_map(const mpark::variant<V...> &value,
    Field field) {
    bool written = false;
    int dummy[] = {0,
      (written = written
          || _try_nested_map(compat::get_if<V>(&value), field),
        0)...};
    (void)dummy;
    return written;
  }

#if defined CXX_STANDARD && 17 <= CXX_STANDARD
  template <typename Field, typename... V>
  static bool _write_field_from_nested_map(const std::variant<V...> &value,
    Field field) {
    bool written = false;
    int dummy[] = {0,
      (written = written
          || _try_nested_map(compat::get_if<V>(&value), field),
        0)...};
    (void)dummy;
    return written;
  }
#endif

  template <typename Field, typename T>
  static bool _write_field_from_nested_map(const T &, Field) {
    return false;
  }
};

#if !defined CXX_STANDARD || CXX_STANDARD <= 11
template <typename V>
struct from_std_map_adapter {
  const std::map<std::string, V> &from;

  template <typename T>
  void operator()(T &to) const {
    auto fields = omni::reflected(to).public_fields();
    omni::compat::apply(*this, fields);
  }

  template <typename... Field>
  void operator()(Field... field) const {
    write_fields_from_std_map{}(from, field...);
  }
};

template <typename T, typename V>
struct from_std_map_return_adapter {
  const std::map<std::string, V> &from;

  T operator()(T to) const {
    auto fields = omni::reflected(to).public_fields();
    omni::compat::apply(*this, fields);
    return to;
  }

  template <typename... Field>
  void operator()(Field... field) const {
    write_fields_from_std_map{}(from, field...);
  }
};
#endif

template <typename T, typename V>
void from_std_map(const std::map<std::string, V> &from, T &to) {
#if defined CXX_STANDARD && 11 < CXX_STANDARD
  omni::reflected_call(
    [](auto &v, const auto &from) -> void {
      auto fields = omni::reflected(v).public_fields();
      omni::compat::apply(
        [&from](auto... field) { write_fields_from_std_map{}(from, field...); },
        fields);
    },
    to,
    from);
#else
  omni::reflected_call(from_std_map_adapter<V>{from}, to);
#endif
}

template <typename T, typename V>
T from_std_map(const std::map<std::string, V> &from) {
#if defined CXX_STANDARD && 11 < CXX_STANDARD
  return omni::reflected_call(
    [](auto type, const auto &from) -> T {
      using out_type = typename decltype(type)::type;
      out_type to{};
      auto fields = omni::reflected(to).public_fields();
      omni::compat::apply(
        [&from](auto... field) { write_fields_from_std_map{}(from, field...); },
        fields);
      return to;
    },
    omni::compat::type_identity<T>{},
    from);
#else
  return omni::reflected_call(from_std_map_return_adapter<T, V>{from}, T{});
#endif
}
} // namespace example_impl

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

namespace example {
struct in_cpp_struct {
  std::string in_cpp_field_0;
  int in_cpp_field_1;
  double in_cpp_field_2;
};

struct in_cpp_struct_with_nested_struct {
  struct nested {
    int i;
  };

  nested n;
  std::string name;
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

class in_cpp_write_mixed_access {
  std::string hidden = "you can't see me";

  public:
  std::string name;
  int count = 0;
  double score = 0.0;

  const std::string &hidden_value() const {
    return hidden;
  }
};

struct in_cpp_scalar_pack {
  bool enabled;
  char code;
  unsigned level;
  std::string label;
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

TEST(print_names, in_cpp_struct_through_tuple_arg) {
  std::tuple<example::in_cpp_struct> p;

  ASSERT_EQ((std::vector<std::string>{
              "in_cpp_field_0",
              "in_cpp_field_1",
              "in_cpp_field_2",
            }),
    omni::reflected_call(example_impl::print_tuple_first_field_names, p));
}

TEST(print_names, in_cpp_second_struct_through_tuple_arg) {
  std::tuple<int, example::in_cpp_struct> p;

  ASSERT_EQ((std::vector<std::string>{
              "in_cpp_field_0",
              "in_cpp_field_1",
              "in_cpp_field_2",
            }),
    omni::reflected_call(example_impl::print_tuple_second_field_names, p));
}

TEST(print_names, not_reflected_int_path_returns_empty_names) {
  ASSERT_EQ((std::vector<std::string>{}),
    example_impl::maybe_print_field_names(8));
}

TEST(print_names, not_reflected_string_path_returns_empty_names) {
  const std::string p = "not reflected";

  ASSERT_EQ((std::vector<std::string>{}),
    example_impl::maybe_print_field_names(p));
}

// FIXME: does not generate: when a reflected_call input is a composed type
// such as std::tuple<std::tuple<unnamed, int>>, the unnamed nested tuple
// element is not promoted to stable reflection metadata. Header mode currently
// only instruments direct std::tuple element types.
//
// TEST(print_names, in_cpp_local_unnamed_struct_through_nested_tuple_arg) {
//   struct {
//     std::string nested_tuple_input_field_0;
//     int nested_tuple_input_field_1;
//     double nested_tuple_input_field_2;
//   } p{};
//
//   const std::tuple<std::tuple<decltype(p), int>> types =
//     std::make_tuple(std::make_tuple(p, 1));
//
//   ASSERT_EQ((std::vector<std::string>{
//               "nested_tuple_input_field_0",
//               "nested_tuple_input_field_1",
//               "nested_tuple_input_field_2",
//             }),
//     omni::reflected_call(example_impl::print_tuple_first_field_names, types));
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
TEST(print_names, in_header_struct) {
  const example::in_header_struct p{};
  const static std::vector<std::string> expected{
    "in_header_field_0",
    "in_header_field_1",
    "in_header_field_2",
  };
  ASSERT_EQ(expected,
    omni::reflected_call(example_impl::print_field_names_simple, p));
}
TEST(print_names, in_cpp_struct) {
  const example::in_cpp_struct p{};
  const static std::vector<std::string> expected{
    "in_cpp_field_0",
    "in_cpp_field_1",
    "in_cpp_field_2",
  };
  ASSERT_EQ(expected,
    omni::reflected_call(example_impl::print_field_names_simple, p));
}

TEST(print_names, in_cpp_derived_from_header_struct) {
  const example::in_cpp_derived_from_header_struct p{};
  const static std::vector<std::string> expected{
    "in_header_field_0",
    "in_header_field_1",
    "in_header_field_2",
    "in_cpp_derived_field_0",
    "in_cpp_derived_field_1",
    "in_cpp_derived_field_2",
  };
  ASSERT_EQ(expected,
    omni::reflected_call(example_impl::print_field_names_simple, p));
}

TEST(print_names, in_cpp_derived_from_cpp_struct) {
  const example::in_cpp_derived_from_cpp_struct p{};
  const static std::vector<std::string> expected{
    "in_cpp_field_0",
    "in_cpp_field_1",
    "in_cpp_field_2",
    "in_cpp_derived_from_cpp_field_0",
    "in_cpp_derived_from_cpp_field_1",
    "in_cpp_derived_from_cpp_field_2",
  };
  ASSERT_EQ(expected,
    omni::reflected_call(example_impl::print_field_names_simple, p));
}

TEST(print_names, in_cpp_multi_base) {
  const example::in_cpp_multi_base p{};
  const static std::vector<std::string> expected{
    "in_header_field_0",
    "in_header_field_1",
    "in_header_field_2",
    "in_cpp_field_0",
    "in_cpp_field_1",
    "in_cpp_field_2",
    "in_cpp_multi_base_field_0",
    "in_cpp_multi_base_field_1",
    "in_cpp_multi_base_field_2",
  };
  ASSERT_EQ(expected,
    omni::reflected_call(example_impl::print_field_names_simple, p));
}

TEST(print_enums, in_cpp_scoped_enum) {
  const example::in_cpp_scoped_enum e{};

  ASSERT_EQ((std::vector<std::string>{
              "in_cpp_scoped_enum_a",
              "in_cpp_scoped_enum_b",
              "in_cpp_scoped_enum_c",
            }),
    omni::reflected_call(get_enumerators, e));
}

TEST(print_enums, in_cpp_fixed_enum) {
  const example::in_cpp_fixed_enum e{};

  ASSERT_EQ((std::vector<std::string>{
              "in_cpp_fixed_enum_a",
              "in_cpp_fixed_enum_b",
              "in_cpp_fixed_enum_c",
            }),
    omni::reflected_call(get_enumerators, e));
}

TEST(print_enums, in_cpp_scoped_enum_with_underlying) {
  const example::in_cpp_scoped_enum_with_underlying e{};

  static const std::vector<std::string> k_expected{
    "in_cpp_scoped_enum_with_underlying_a",
    "in_cpp_scoped_enum_with_underlying_b",
    "in_cpp_scoped_enum_with_underlying_c",
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

// FIXME: does not generate: when a reflected_call input is a composed type
// such as std::tuple<std::tuple<unnamed, int>>, the unnamed nested tuple
// element is not instrumented as a reflected type. The current tuple overload
// only instruments direct std::tuple element types.
//
// TEST(read_values, in_cpp_local_unnamed_struct_through_nested_tuple_arg) {
//   struct {
//     std::string read_nested_tuple_name;
//     int read_nested_tuple_count;
//     double read_nested_tuple_score;
//   } p{"nested tuple reader", 46, 46.5};
//
//   const std::tuple<std::tuple<decltype(p), int>> types =
//     std::make_tuple(std::make_tuple(p, 1));
//
//   ASSERT_EQ((std::vector<std::string>{
//               "nested tuple reader",
//               "46",
//               "46.5",
//             }),
//     omni::reflected_call([](const auto &types) {
//       const auto fields = omni::reflected(std::get<0>(std::get<0>(types)))
//                             .public_fields();
//       return omni::compat::apply(example_impl::_field_values{}, fields);
//     }, types));
// }

TEST(read_values, in_cpp_struct) {
  example::in_cpp_struct p{};
  p.in_cpp_field_0 = "named";
  p.in_cpp_field_1 = 136;
  p.in_cpp_field_2 = 36.5;

  ASSERT_EQ((std::vector<std::string>{
              "named",
              "136",
              "36.5",
            }),
    omni::reflected_call(example_impl::field_values_simple, p));
}

TEST(read_values, in_header_struct) {
  example::in_header_struct p{};
  p.in_header_field_0 = "header read";
  p.in_header_field_1 = 137;
  p.in_header_field_2 = 37.5;

  ASSERT_EQ((std::vector<std::string>{
              "header read",
              "137",
              "37.5",
            }),
    omni::reflected_call(example_impl::field_values_simple, p));
}

TEST(read_values, in_cpp_derived_from_header_struct) {
  example::in_cpp_derived_from_header_struct p{};
  p.in_header_field_0 = "base read";
  p.in_header_field_1 = 138;
  p.in_header_field_2 = 38.5;
  p.in_cpp_derived_field_0 = "derived read";
  p.in_cpp_derived_field_1 = 139;
  p.in_cpp_derived_field_2 = 39.5;

  ASSERT_EQ((std::vector<std::string>{
              "base read",
              "138",
              "38.5",
              "derived read",
              "139",
              "39.5",
            }),
    omni::reflected_call(example_impl::field_values_simple, p));
}

TEST(read_values, in_cpp_private_base_public_only) {
  example::in_cpp_private_base p{};
  p.in_cpp_private_base_field_0 = "private read";
  p.in_cpp_private_base_field_1 = 140;
  p.in_cpp_private_base_field_2 = 40.5;

  ASSERT_EQ((std::vector<std::string>{
              "private read",
              "140",
              "40.5",
            }),
    omni::reflected_call(example_impl::field_values_simple, p));
}

TEST(read_values, in_cpp_mixed_access_public_only) {
  example::in_cpp_write_mixed_access p{};
  p.name = "John Cena";
  p.count = 141;
  p.score = 41.5;

  ASSERT_EQ((std::vector<std::string>{
              "John Cena",
              "141",
              "41.5",
            }),
    omni::reflected_call(example_impl::field_values_simple, p));
}

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

// FIXME: compiles but omits transitive public base fields inherited through the
// direct reflected base.
//
TEST(print_names, DISABLED_in_cpp_deep_public_base_chain) {
  const example::in_cpp_deep_derived p{};
  const static std::vector<std::string> expected{
    "in_header_field_0",
    "in_header_field_1",
    "in_header_field_2",
    "in_cpp_mid_field_0",
    "in_cpp_mid_field_1",
    "in_cpp_mid_field_2",
    "in_cpp_deep_field_0",
    "in_cpp_deep_field_1",
    "in_cpp_deep_field_2",
  };
  ASSERT_EQ(expected,
    omni::reflected_call(example_impl::print_field_names_simple, p));
}

// FIXME: compiles but omits transitive public base fields inherited through the
// direct reflected base.
//
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
// sequence templates. Some of these can be generated via decltype(member
// field).
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
// FIXME(high): does not compile in header mode in this large translation unit:
// same-shape local unnamed direct reflected_call records generate correct
// metadata, but the real compilation can match an earlier incompatible indexed
// specialization. This is not bitfield-specific; observed first test metadata
// generated at index 62 with fields `foo_count`, `bar_count`,
// `untouched_count`, while real compilation matched index 53, generated for
// `write_values.in_cpp_local_unnamed_struct_from_std_map` with fields `name`,
// `count`, `score`. A standalone two-route probe keeps stable indexes under
// Clang and GCC with the generated header force-included, so the trigger needs
// accumulated header-mode TU context.
// TEST(write_values, in_cpp_local_unnamed_struct_foo_bar_first_identical) {
//   struct {
//     int foo_count;
//     int bar_count;
//     int untouched_count;
//   } p{1, 2, 3};
//
//   omni::reflected_call([](auto &v) {
//     const auto fields = omni::reflected(v).public_fields();
//     omni::compat::apply(example_impl::_write_foo_bar{}, fields);
//   }, p);
//
//   ASSERT_EQ(8, p.foo_count);
//   ASSERT_EQ(15, p.bar_count);
//   ASSERT_EQ(3, p.untouched_count);
// }
//
// TEST(write_values, in_cpp_local_unnamed_struct_foo_bar_second_identical) {
//   struct {
//     int foo_count;
//     int bar_count;
//     int untouched_count;
//   } p{4, 5, 6};
//
//   omni::reflected_call([](auto &v) {
//     const auto fields = omni::reflected(v).public_fields();
//     omni::compat::apply(example_impl::_write_foo_bar{}, fields);
//   }, p);
//
//   ASSERT_EQ(8, p.foo_count);
//   ASSERT_EQ(15, p.bar_count);
//   ASSERT_EQ(6, p.untouched_count);
// }

// FIXME(high): does not compile in header mode: broader direct local unnamed
// write visitor route for the same indexed mismatch reproduced by the
// same-shape regression above.
//
// TEST(write_values, in_cpp_local_unnamed_struct_foo_bar_visitor) {
//   struct {
//     int foo_count;
//     int bar_count;
//     int untouched_count;
//     std::string foo_name;
//   } p{1, 2, 3, "same"};
//
//   omni::reflected_call([](auto &v) {
//     const auto fields = omni::reflected(v).public_fields();
//     omni::compat::apply(example_impl::_write_foo_bar{}, fields);
//   }, p);
//
//   ASSERT_EQ(8, p.foo_count);
//   ASSERT_EQ(15, p.bar_count);
//   ASSERT_EQ(3, p.untouched_count);
//   ASSERT_EQ("same", p.foo_name);
// }
//
// TEST(write_values, in_cpp_local_unnamed_struct_foo_bar_mixed_order) {
//   struct {
//     int bar_first;
//     std::string title;
//     int foo_second;
//     double foo_score;
//   } p{1, "unchanged", 2, 3.5};
//
//   omni::reflected_call([](auto &v) {
//     const auto fields = omni::reflected(v).public_fields();
//     omni::compat::apply(example_impl::_write_foo_bar{}, fields);
//   }, p);
//
//   ASSERT_EQ(15, p.bar_first);
//   ASSERT_EQ("unchanged", p.title);
//   ASSERT_EQ(8, p.foo_second);
//   ASSERT_EQ(3.5, p.foo_score);
// }

// FIXME: does not compile: local class types inside TEST bodies are rendered
// through the GTest fixture class before that class is declared in the forced
// include. Expected behavior is to reflect only the public fields.
//
// TEST(write_values, in_cpp_local_unnamed_struct_with_mixed_access_from_std_map) {
//   class local_unnamed_like {
//     int hidden = 1;
//
//     public:
//     std::string name;
//     int count;
//     double score;
//
//     int hidden_value() const {
//       return hidden;
//     }
//   } p{};
//
//   std::map<std::string, mpark::variant<int, double, std::string>> from;
//   from["name"] = std::string{"mixed"};
//   from["count"] = 112;
//   from["score"] = 12.2;
//   from["hidden"] = 99;
//   example_impl::from_std_map(from, p);
//
//   ASSERT_EQ("mixed", p.name);
//   ASSERT_EQ(112, p.count);
//   ASSERT_EQ(12.2, p.score);
//   ASSERT_EQ(1, p.hidden_value());
// }

// FIXME(high): does not compile in header mode: bitfield variant of the same
// direct local unnamed indexed mismatch reproduced by the same-shape regression
// above. Bitfields are not the root trigger.
//
// TEST(write_values, in_cpp_local_unnamed_bitfields_foo_bar_visitor) {
//   struct {
//     unsigned foo_bits : 4;
//     unsigned bar_bits : 5;
//     int plain;
//   } p{};
//
//   omni::reflected_call([](auto &v) {
//     const auto fields = omni::reflected(v).public_fields();
//     omni::compat::apply(example_impl::_write_foo_bar{}, fields);
//   }, p);
//
//   ASSERT_EQ(8u, p.foo_bits);
//   ASSERT_EQ(15u, p.bar_bits);
//   ASSERT_EQ(0, p.plain);
// }

TEST(write_values, in_cpp_struct_from_std_map_by_reference) {
  example::in_cpp_struct p{};

  std::map<std::string, mpark::variant<int, double, std::string>> from;
  from["in_cpp_field_0"] = std::string{"referenced"};
  from["in_cpp_field_1"] = 114;
  from["in_cpp_field_2"] = 14.4;
  example_impl::from_std_map(from, p);

  ASSERT_EQ("referenced", p.in_cpp_field_0);
  ASSERT_EQ(114, p.in_cpp_field_1);
  ASSERT_EQ(14.4, p.in_cpp_field_2);
}

TEST(write_values, in_cpp_struct_from_std_map_missing_field) {
  example::in_cpp_struct p{"old", 142, 42.5};

  std::map<std::string, mpark::variant<int, double, std::string>> from;
  from["in_cpp_field_0"] = std::string{"partial"};
  from["in_cpp_field_2"] = 43.5;
  example_impl::from_std_map(from, p);

  ASSERT_EQ("partial", p.in_cpp_field_0);
  ASSERT_EQ(142, p.in_cpp_field_1);
  ASSERT_EQ(43.5, p.in_cpp_field_2);
}

TEST(write_values, in_cpp_struct_from_std_map_wrong_type) {
  example::in_cpp_struct p{"old", 143, 43.5};

  std::map<std::string, mpark::variant<int, double, std::string>> from;
  from["in_cpp_field_0"] = 144;
  from["in_cpp_field_1"] = std::string{"wrong"};
  from["in_cpp_field_2"] = 44.5;
  example_impl::from_std_map(from, p);

  ASSERT_EQ("old", p.in_cpp_field_0);
  ASSERT_EQ(143, p.in_cpp_field_1);
  ASSERT_EQ(44.5, p.in_cpp_field_2);
}

TEST(write_values, in_cpp_struct_from_std_map_extra_keys) {
  example::in_cpp_struct p{};

  std::map<std::string, mpark::variant<int, double, std::string>> from;
  from["in_cpp_field_0"] = std::string{"extra"};
  from["in_cpp_field_1"] = 145;
  from["in_cpp_field_2"] = 45.5;
  from["not_a_field"] = std::string{"ignored"};
  example_impl::from_std_map(from, p);

  ASSERT_EQ("extra", p.in_cpp_field_0);
  ASSERT_EQ(145, p.in_cpp_field_1);
  ASSERT_EQ(45.5, p.in_cpp_field_2);
}

TEST(write_values, in_header_struct_from_std_map_by_reference) {
  example::in_header_struct p{};

  std::map<std::string, mpark::variant<int, double, std::string>> from;
  from["in_header_field_0"] = std::string{"header"};
  from["in_header_field_1"] = 120;
  from["in_header_field_2"] = 20.5;
  example_impl::from_std_map(from, p);

  ASSERT_EQ("header", p.in_header_field_0);
  ASSERT_EQ(120, p.in_header_field_1);
  ASSERT_EQ(20.5, p.in_header_field_2);
}

TEST(write_values, in_cpp_derived_from_header_struct_from_std_map) {
  example::in_cpp_derived_from_header_struct p{};

  std::map<std::string, mpark::variant<int, double, std::string>> from;
  from["in_header_field_0"] = std::string{"base header"};
  from["in_header_field_1"] = 121;
  from["in_header_field_2"] = 21.5;
  from["in_cpp_derived_field_0"] = std::string{"derived"};
  from["in_cpp_derived_field_1"] = 122;
  from["in_cpp_derived_field_2"] = 22.5;
  example_impl::from_std_map(from, p);

  ASSERT_EQ("base header", p.in_header_field_0);
  ASSERT_EQ(121, p.in_header_field_1);
  ASSERT_EQ(21.5, p.in_header_field_2);
  ASSERT_EQ("derived", p.in_cpp_derived_field_0);
  ASSERT_EQ(122, p.in_cpp_derived_field_1);
  ASSERT_EQ(22.5, p.in_cpp_derived_field_2);
}

TEST(write_values, in_cpp_derived_from_cpp_struct_from_std_map) {
  example::in_cpp_derived_from_cpp_struct p{};

  std::map<std::string, mpark::variant<int, double, std::string>> from;
  from["in_cpp_field_0"] = std::string{"base cpp"};
  from["in_cpp_field_1"] = 123;
  from["in_cpp_field_2"] = 23.5;
  from["in_cpp_derived_from_cpp_field_0"] = std::string{"derived cpp"};
  from["in_cpp_derived_from_cpp_field_1"] = 124;
  from["in_cpp_derived_from_cpp_field_2"] = 24.5;
  example_impl::from_std_map(from, p);

  ASSERT_EQ("base cpp", p.in_cpp_field_0);
  ASSERT_EQ(123, p.in_cpp_field_1);
  ASSERT_EQ(23.5, p.in_cpp_field_2);
  ASSERT_EQ("derived cpp", p.in_cpp_derived_from_cpp_field_0);
  ASSERT_EQ(124, p.in_cpp_derived_from_cpp_field_1);
  ASSERT_EQ(24.5, p.in_cpp_derived_from_cpp_field_2);
}

TEST(write_values, in_cpp_multi_base_from_std_map) {
  example::in_cpp_multi_base p{};

  std::map<std::string, mpark::variant<int, double, std::string>> from;
  from["in_header_field_0"] = std::string{"multi header"};
  from["in_header_field_1"] = 125;
  from["in_header_field_2"] = 25.5;
  from["in_cpp_field_0"] = std::string{"multi cpp"};
  from["in_cpp_field_1"] = 126;
  from["in_cpp_field_2"] = 26.5;
  from["in_cpp_multi_base_field_0"] = std::string{"multi"};
  from["in_cpp_multi_base_field_1"] = 127;
  from["in_cpp_multi_base_field_2"] = 27.5;
  example_impl::from_std_map(from, p);

  ASSERT_EQ("multi header", p.in_header_field_0);
  ASSERT_EQ(125, p.in_header_field_1);
  ASSERT_EQ(25.5, p.in_header_field_2);
  ASSERT_EQ("multi cpp", p.in_cpp_field_0);
  ASSERT_EQ(126, p.in_cpp_field_1);
  ASSERT_EQ(26.5, p.in_cpp_field_2);
  ASSERT_EQ("multi", p.in_cpp_multi_base_field_0);
  ASSERT_EQ(127, p.in_cpp_multi_base_field_1);
  ASSERT_EQ(27.5, p.in_cpp_multi_base_field_2);
}

TEST(write_values, in_cpp_private_base_from_std_map_public_only) {
  example::in_cpp_private_base p{};

  std::map<std::string, mpark::variant<int, double, std::string>> from;
  from["in_cpp_field_0"] = std::string{"hidden base"};
  from["in_cpp_field_1"] = 128;
  from["in_cpp_field_2"] = 28.5;
  from["in_cpp_private_base_field_0"] = std::string{"private own"};
  from["in_cpp_private_base_field_1"] = 129;
  from["in_cpp_private_base_field_2"] = 29.5;
  example_impl::from_std_map(from, p);

  ASSERT_EQ("private own", p.in_cpp_private_base_field_0);
  ASSERT_EQ(129, p.in_cpp_private_base_field_1);
  ASSERT_EQ(29.5, p.in_cpp_private_base_field_2);
}

TEST(write_values, in_cpp_protected_base_from_std_map_public_only) {
  example::in_cpp_protected_base p{};

  std::map<std::string, mpark::variant<int, double, std::string>> from;
  from["in_header_field_0"] = std::string{"hidden header"};
  from["in_header_field_1"] = 130;
  from["in_header_field_2"] = 30.5;
  from["in_cpp_protected_base_field_0"] = std::string{"protected own"};
  from["in_cpp_protected_base_field_1"] = 131;
  from["in_cpp_protected_base_field_2"] = 31.5;
  example_impl::from_std_map(from, p);

  ASSERT_EQ("protected own", p.in_cpp_protected_base_field_0);
  ASSERT_EQ(131, p.in_cpp_protected_base_field_1);
  ASSERT_EQ(31.5, p.in_cpp_protected_base_field_2);
}

TEST(write_values, in_cpp_mixed_access_from_std_map_public_only) {
  example::in_cpp_write_mixed_access p{};

  std::map<std::string, mpark::variant<int, double, std::string>> from;
  from["hidden"] = std::string{"changed"};
  from["name"] = std::string{"John Cena"};
  from["count"] = 132;
  from["score"] = 32.5;
  example_impl::from_std_map(from, p);

  ASSERT_EQ("you can't see me", p.hidden_value());
  ASSERT_EQ("John Cena", p.name);
  ASSERT_EQ(132, p.count);
  ASSERT_EQ(32.5, p.score);
}

TEST(write_values, in_cpp_scalar_pack_from_std_map) {
  example::in_cpp_scalar_pack p{};

  std::map<std::string, mpark::variant<bool, char, unsigned, std::string>> from;
  from["enabled"] = true;
  from["code"] = 's';
  from["level"] = 149u;
  from["label"] = std::string{"scalar"};
  example_impl::from_std_map(from, p);

  ASSERT_EQ(true, p.enabled);
  ASSERT_EQ('s', p.code);
  ASSERT_EQ(149u, p.level);
  ASSERT_EQ("scalar", p.label);
}

TEST(write_values, in_cpp_scalar_pack_from_std_map_missing_field) {
  example::in_cpp_scalar_pack p{false, 'm', 150u, "old"};

  std::map<std::string, mpark::variant<bool, char, unsigned, std::string>> from;
  from["enabled"] = true;
  from["label"] = std::string{"partial scalar"};
  example_impl::from_std_map(from, p);

  ASSERT_EQ(true, p.enabled);
  ASSERT_EQ('m', p.code);
  ASSERT_EQ(150u, p.level);
  ASSERT_EQ("partial scalar", p.label);
}

TEST(write_values, in_cpp_scalar_pack_from_std_map_wrong_type) {
  example::in_cpp_scalar_pack p{false, 'w', 151u, "old"};

  std::map<std::string, mpark::variant<bool, char, unsigned, std::string>> from;
  from["enabled"] = std::string{"wrong"};
  from["code"] = 152u;
  from["level"] = 'x';
  from["label"] = std::string{"valid"};
  example_impl::from_std_map(from, p);

  ASSERT_EQ(false, p.enabled);
  ASSERT_EQ('w', p.code);
  ASSERT_EQ(151u, p.level);
  ASSERT_EQ("valid", p.label);
}

TEST(write_values, in_cpp_scalar_pack_from_std_map_extra_keys) {
  example::in_cpp_scalar_pack p{};

  std::map<std::string, mpark::variant<bool, char, unsigned, std::string>> from;
  from["enabled"] = true;
  from["code"] = 'e';
  from["level"] = 153u;
  from["label"] = std::string{"extra scalar"};
  from["not_a_field"] = std::string{"ignored"};
  example_impl::from_std_map(from, p);

  ASSERT_EQ(true, p.enabled);
  ASSERT_EQ('e', p.code);
  ASSERT_EQ(153u, p.level);
  ASSERT_EQ("extra scalar", p.label);
}

// FIXME: disabled: header mode reflects direct public base fields, but not
// transitive public base fields. The in_header_struct grand-base fields remain
// unchanged at runtime.
TEST(write_values, DISABLED_in_cpp_deep_derived_from_std_map) {
  example::in_cpp_deep_derived p{};

  std::map<std::string, mpark::variant<int, double, std::string>> from;
  from["in_header_field_0"] = std::string{"deep header"};
  from["in_header_field_1"] = 133;
  from["in_header_field_2"] = 33.5;
  from["in_cpp_mid_field_0"] = std::string{"mid"};
  from["in_cpp_mid_field_1"] = 134;
  from["in_cpp_mid_field_2"] = 34.5;
  from["in_cpp_deep_field_0"] = std::string{"deep"};
  from["in_cpp_deep_field_1"] = 135;
  from["in_cpp_deep_field_2"] = 35.5;
  example_impl::from_std_map(from, p);

  ASSERT_EQ("deep header", p.in_header_field_0);
  ASSERT_EQ(133, p.in_header_field_1);
  ASSERT_EQ(33.5, p.in_header_field_2);
  ASSERT_EQ("mid", p.in_cpp_mid_field_0);
  ASSERT_EQ(134, p.in_cpp_mid_field_1);
  ASSERT_EQ(34.5, p.in_cpp_mid_field_2);
  ASSERT_EQ("deep", p.in_cpp_deep_field_0);
  ASSERT_EQ(135, p.in_cpp_deep_field_1);
  ASSERT_EQ(35.5, p.in_cpp_deep_field_2);
}

TEST(write_values, in_cpp_struct_with_nested_mpark_variant_map) {
  using nested_type = example::in_cpp_struct_with_nested_struct::nested;
  using nested_value = mpark::variant<int, double, std::string>;
  using nested_map = std::map<std::string, nested_value>;
  using value = mpark::variant<int, double, std::string, nested_type, nested_map>;

  example::in_cpp_struct_with_nested_struct p{};

  nested_map nested;
  nested["i"] = 815;

  std::map<std::string, value> from;
  from["n"] = nested;
  from["name"] = std::string{"nested"};
  example_impl::from_std_map(from, p);

  ASSERT_EQ(815, p.n.i);
  ASSERT_EQ("nested", p.name);
}

TEST(write_values, in_cpp_struct_with_nested_mpark_flat_map) {
  using nested_type = example::in_cpp_struct_with_nested_struct::nested;
  example::in_cpp_struct_with_nested_struct p{};

  std::map<std::string, mpark::variant<int, double, std::string, nested_type>>
    from;
  from["i"] = 115;
  from["name"] = std::string{"flat nested"};
  example_impl::from_std_map(from, p);

  ASSERT_EQ(115, p.n.i);
  ASSERT_EQ("flat nested", p.name);
}

TEST(write_values, in_cpp_struct_with_nested_mpark_map_wrong_nested_type) {
  using nested_type = example::in_cpp_struct_with_nested_struct::nested;
  using nested_value = mpark::variant<int, double, std::string>;
  using nested_map = std::map<std::string, nested_value>;
  using value = mpark::variant<int, double, std::string, nested_type, nested_map>;

  example::in_cpp_struct_with_nested_struct p{};

  std::map<std::string, value> from;
  from["n"] = std::string{"not a nested map"};
  from["i"] = 116;
  from["name"] = std::string{"fallback nested"};
  example_impl::from_std_map(from, p);

  ASSERT_EQ(116, p.n.i);
  ASSERT_EQ("fallback nested", p.name);
}

TEST(write_values, in_cpp_struct_with_nested_mpark_map_missing_nested_field) {
  using nested_type = example::in_cpp_struct_with_nested_struct::nested;
  using nested_value = mpark::variant<int, double, std::string>;
  using nested_map = std::map<std::string, nested_value>;
  using value = mpark::variant<int, double, std::string, nested_type, nested_map>;

  example::in_cpp_struct_with_nested_struct p{};
  p.n.i = 146;

  nested_map nested;

  std::map<std::string, value> from;
  from["n"] = nested;
  from["name"] = std::string{"missing nested"};
  example_impl::from_std_map(from, p);

  ASSERT_EQ(0, p.n.i);
  ASSERT_EQ("missing nested", p.name);
}

TEST(write_values, in_cpp_struct_with_nested_mpark_map_extra_nested_keys) {
  using nested_type = example::in_cpp_struct_with_nested_struct::nested;
  using nested_value = mpark::variant<int, double, std::string>;
  using nested_map = std::map<std::string, nested_value>;
  using value = mpark::variant<int, double, std::string, nested_type, nested_map>;

  example::in_cpp_struct_with_nested_struct p{};

  nested_map nested;
  nested["i"] = 147;
  nested["ignored"] = std::string{"extra nested"};

  std::map<std::string, value> from;
  from["n"] = nested;
  from["name"] = std::string{"extra nested"};
  example_impl::from_std_map(from, p);

  ASSERT_EQ(147, p.n.i);
  ASSERT_EQ("extra nested", p.name);
}

TEST(write_values, in_cpp_struct_with_nested_mpark_map_wrong_nested_value_type) {
  using nested_type = example::in_cpp_struct_with_nested_struct::nested;
  using nested_value = mpark::variant<int, double, std::string>;
  using nested_map = std::map<std::string, nested_value>;
  using value = mpark::variant<int, double, std::string, nested_type, nested_map>;

  example::in_cpp_struct_with_nested_struct p{};

  nested_map nested;
  nested["i"] = std::string{"wrong nested value"};

  std::map<std::string, value> from;
  from["n"] = nested;
  from["name"] = std::string{"wrong nested value"};
  example_impl::from_std_map(from, p);

  ASSERT_EQ(0, p.n.i);
  ASSERT_EQ("wrong nested value", p.name);
}

// FIXME: does not compile: local named nested dependency types are rendered
// through the GTest fixture scope before that scope is declared in the forced
// include.
//
// TEST(write_values, in_cpp_local_unnamed_struct_with_nested_mpark_map) {
//   struct nested {
//     int i;
//   };
//
//   struct {
//     nested n;
//     std::string name;
//   } p{};
//
//   std::map<std::string, mpark::variant<int, double, std::string>> from;
//   from["i"] = 117;
//   from["name"] = std::string{"local nested"};
//   example_impl::from_std_map(from, p);
//
//   ASSERT_EQ(117, p.n.i);
//   ASSERT_EQ("local nested", p.name);
// }

#if defined CXX_STANDARD && 17 <= CXX_STANDARD
TEST(write_values, in_cpp_struct_with_nested_std_variant_map) {
  using nested_type = example::in_cpp_struct_with_nested_struct::nested;
  using nested_value = std::variant<int, double, std::string>;
  using nested_map = std::map<std::string, nested_value>;
  using value = std::variant<int, double, std::string, nested_type, nested_map>;

  example::in_cpp_struct_with_nested_struct p{};

  nested_map nested;
  nested["i"] = 916;

  std::map<std::string, value> from;
  from["n"] = nested;
  from["name"] = std::string{"standard nested"};
  example_impl::from_std_map(from, p);

  ASSERT_EQ(916, p.n.i);
  ASSERT_EQ("standard nested", p.name);
}

// FIXME(high): does not compile in packaged clang header-mode builds for
// C++17 and later. Local unnamed std::variant map routes are detected by the
// tool, but the real compilation does not find a matching generated _reflected
// specialization for the local unnamed record. This is not an indexed unique-id
// order issue; reflected-scope index queries are non-mutating.
//
// TEST(write_values, in_cpp_local_unnamed_struct_from_std_variant_map_reference)
// {
//   struct {
//     std::string name;
//     int count;
//     double score;
//   } p{};
//
//   std::map<std::string, std::variant<int, double, std::string>> from;
//   from["name"] = std::string{"standard"};
//   from["count"] = 64;
//   from["score"] = 11.5;
//   example_impl::from_std_map(from, p);
//
//   ASSERT_EQ("standard", p.name);
//   ASSERT_EQ(64, p.count);
//   ASSERT_EQ(11.5, p.score);
// }
//
// TEST(write_values, in_cpp_local_unnamed_struct_from_std_variant_map_wrong_type)
// {
//   struct {
//     std::string name;
//     int count;
//     double score;
//   } p{"old", 65, 12.5};
//
//   std::map<std::string, std::variant<int, double, std::string>> from;
//   from["name"] = 15;
//   from["count"] = std::string{"wrong"};
//   from["score"] = 13.5;
//   example_impl::from_std_map(from, p);
//
//   ASSERT_EQ("old", p.name);
//   ASSERT_EQ(65, p.count);
//   ASSERT_EQ(13.5, p.score);
// }

TEST(write_values, in_cpp_scalar_pack_from_std_variant_map) {
  example::in_cpp_scalar_pack p{};

  std::map<std::string, std::variant<bool, char, unsigned, std::string>> from;
  from["enabled"] = true;
  from["code"] = 'v';
  from["level"] = 154u;
  from["label"] = std::string{"std scalar"};
  example_impl::from_std_map(from, p);

  ASSERT_EQ(true, p.enabled);
  ASSERT_EQ('v', p.code);
  ASSERT_EQ(154u, p.level);
  ASSERT_EQ("std scalar", p.label);
}

TEST(write_values, in_cpp_scalar_pack_from_std_variant_map_wrong_type) {
  example::in_cpp_scalar_pack p{false, 'z', 155u, "old"};

  std::map<std::string, std::variant<bool, char, unsigned, std::string>> from;
  from["enabled"] = std::string{"wrong"};
  from["code"] = 156u;
  from["level"] = 'y';
  from["label"] = std::string{"std valid"};
  example_impl::from_std_map(from, p);

  ASSERT_EQ(false, p.enabled);
  ASSERT_EQ('z', p.code);
  ASSERT_EQ(155u, p.level);
  ASSERT_EQ("std valid", p.label);
}

TEST(write_values, in_cpp_struct_with_nested_std_flat_map) {
  using nested_type = example::in_cpp_struct_with_nested_struct::nested;
  example::in_cpp_struct_with_nested_struct p{};

  std::map<std::string, std::variant<int, double, std::string, nested_type>>
    from;
  from["i"] = 118;
  from["name"] = std::string{"std flat nested"};
  example_impl::from_std_map(from, p);

  ASSERT_EQ(118, p.n.i);
  ASSERT_EQ("std flat nested", p.name);
}

TEST(write_values, in_cpp_struct_with_nested_std_map_extra_nested_keys) {
  using nested_type = example::in_cpp_struct_with_nested_struct::nested;
  using nested_value = std::variant<int, double, std::string>;
  using nested_map = std::map<std::string, nested_value>;
  using value = std::variant<int, double, std::string, nested_type, nested_map>;

  example::in_cpp_struct_with_nested_struct p{};

  nested_map nested;
  nested["i"] = 148;
  nested["ignored"] = std::string{"std extra nested"};

  std::map<std::string, value> from;
  from["n"] = nested;
  from["name"] = std::string{"std extra nested"};
  example_impl::from_std_map(from, p);

  ASSERT_EQ(148, p.n.i);
  ASSERT_EQ("std extra nested", p.name);
}

TEST(write_values, in_cpp_struct_with_nested_std_map_wrong_nested_value_type) {
  using nested_type = example::in_cpp_struct_with_nested_struct::nested;
  using nested_value = std::variant<int, double, std::string>;
  using nested_map = std::map<std::string, nested_value>;
  using value = std::variant<int, double, std::string, nested_type, nested_map>;

  example::in_cpp_struct_with_nested_struct p{};

  nested_map nested;
  nested["i"] = std::string{"std wrong nested value"};

  std::map<std::string, value> from;
  from["n"] = nested;
  from["name"] = std::string{"std wrong nested value"};
  example_impl::from_std_map(from, p);

  ASSERT_EQ(0, p.n.i);
  ASSERT_EQ("std wrong nested value", p.name);
}
#endif

// TODO: support nested type fields declared inside a local unnamed parent.
// Header mode currently generates an invalid nested type reference for this
// route.
//
// TEST(write_values, in_cpp_local_unnamed_struct_with_nested_struct_from_std_map) {
//   struct {
//     struct nested {
//       int i;
//     };
//
//     nested n;
//     std::string name;
//   } p{};
//
//   std::map<std::string, mpark::variant<int, double, std::string>> from;
//   from["i"] = 815;
//   from["name"] = std::string{"nested"};
//   example_impl::from_std_map(from, p);
//
//   ASSERT_EQ(815, p.n.i);
//   ASSERT_EQ("nested", p.name);
// }

TEST(write_values, in_cpp_struct_with_nested_struct_from_std_map) {
  using nested_type = example::in_cpp_struct_with_nested_struct::nested;

  std::map<std::string, mpark::variant<int, double, std::string, nested_type>>
    from;
  from["i"] = 815;
  from["name"] = std::string{"nested"};

  const example::in_cpp_struct_with_nested_struct p =
    example_impl::from_std_map<example::in_cpp_struct_with_nested_struct>(from);

  ASSERT_EQ(815, p.n.i);
  ASSERT_EQ("nested", p.name);
}

TEST(write_values, in_cpp_named_struct_from_std_map) {
  std::map<std::string, mpark::variant<int, double, std::string>> from;
  from["in_cpp_field_0"] = std::string{"constructed"};
  from["in_cpp_field_1"] = 23;
  from["in_cpp_field_2"] = 42.5;

  const example::in_cpp_struct p =
    example_impl::from_std_map<example::in_cpp_struct>(from);

  ASSERT_EQ("constructed", p.in_cpp_field_0);
  ASSERT_EQ(23, p.in_cpp_field_1);
  ASSERT_EQ(42.5, p.in_cpp_field_2);
}

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
