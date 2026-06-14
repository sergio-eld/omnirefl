#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <omnirefl/reflected_scope.hpp>

namespace interface_test {

struct record_type_t {
  int first;
  std::string second;
};

enum class enum_type_t {
  zero,
  one,
};

namespace nested {

struct namespaced_record_t {
  int value;
};

enum class namespaced_enum_t {
  first,
  second,
};

struct namespaced_field_types_t {
  namespaced_record_t record;
  namespaced_enum_t state;
};

namespace left {
struct duplicate_name_t {
  int value;
};
} // namespace left

namespace right {
struct duplicate_name_t {
  int value;
};
} // namespace right

struct duplicate_leaf_field_types_t {
  left::duplicate_name_t left;
  right::duplicate_name_t right;
};

struct sized_integer_field_types_t {
  std::uint16_t unsigned_16;
  std::int32_t signed_32;
  std::uint64_t *unsigned_64_ptr;
  std::int16_t **signed_16_ptr_ptr;
};

} // namespace nested

namespace type_identity {

// record: omni::reflected_record_t<T>
struct record_type_name_reflected_record_t_t {
  template <typename T>
  std::string operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected_record_t<T>::name();
  }
} const static record_type_name_reflected_record_t{};

// record: omni::reflected_record<T>()
struct record_type_name_reflected_record_fn_t {
  template <typename T>
  std::string operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected_record<T>().name();
  }
} const static record_type_name_reflected_record_fn{};

// record: omni::reflected_record(t)
struct record_type_name_reflected_record_lv_t {
  template <typename T>
  std::string operator()(T &&t) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected_record(std::forward<T>(t)).name();
  }
} const static record_type_name_reflected_record_lv{};

// record: omni::reflected_t<T>
struct record_type_name_reflected_t_t {
  template <typename T>
  std::string operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected_t<T>::name();
  }
} const static record_type_name_reflected_t{};

// record: omni::reflected<T>()
struct record_type_name_reflected_fn_t {
  template <typename T>
  std::string operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected<T>().name();
  }
} const static record_type_name_reflected_fn{};

// record: omni::reflected(t)
struct record_type_name_reflected_lv_t {
  template <typename T>
  std::string operator()(T &&t) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected(std::forward<T>(t)).name();
  }
} const static record_type_name_reflected_lv{};

// enum: omni::reflected_enum_t<T>
struct enum_type_name_reflected_enum_t_t {
  template <typename T>
  std::string operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected_enum_t<T>::name();
  }
} const static enum_type_name_reflected_enum_t{};

// enum: omni::reflected_enum<T>()
struct enum_type_name_reflected_enum_fn_t {
  template <typename T>
  std::string operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected_enum<T>().name();
  }
} const static enum_type_name_reflected_enum_fn{};

// enum: omni::reflected_enum(e)
struct enum_type_name_reflected_enum_lv_t {
  template <typename T>
  std::string operator()(T &&e) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected_enum(std::forward<T>(e)).name();
  }
} const static enum_type_name_reflected_enum_lv{};

// enum: omni::reflected_t<T>
struct enum_type_name_reflected_t_t {
  template <typename T>
  std::string operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected_t<T>::name();
  }
} const static enum_type_name_reflected_t{};

// enum: omni::reflected<T>()
struct enum_type_name_reflected_fn_t {
  template <typename T>
  std::string operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected<T>().name();
  }
} const static enum_type_name_reflected_fn{};

// enum: omni::reflected(e)
struct enum_type_name_reflected_lv_t {
  template <typename T>
  std::string operator()(T &&e) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected(std::forward<T>(e)).name();
  }
} const static enum_type_name_reflected_lv{};

} // namespace type_identity

namespace enumerators {

// enum: omni::reflected_enum_t<T>::enumerators()
struct enum_type_enumerators_reflected_enum_t_t {
  template <typename T>
  std::string operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";

    auto es = omni::reflected_enum_t<T>::enumerators();
    std::string result;
    bool first = true;
    for (const auto &p : es) {
      if (!first)
        result += ',';
      result += p.second;
      first = false;
    }
    return result;
  }
} const static enum_type_enumerators_reflected_enum_t{};

// enum: omni::reflected_enum<T>().enumerators()
struct enum_type_enumerators_reflected_enum_fn_t {
  template <typename T>
  std::string operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";

    auto es = omni::reflected_enum<T>().enumerators();
    std::string result;
    bool first = true;
    for (const auto &p : es) {
      if (!first)
        result += ',';
      result += p.second;
      first = false;
    }
    return result;
  }
} const static enum_type_enumerators_reflected_enum_fn{};

// enum: omni::reflected_enum(e).enumerators()
struct enum_type_enumerators_reflected_enum_lv_t {
  template <typename T>
  std::string operator()(T &&e) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";

    auto es = omni::reflected_enum(std::forward<T>(e)).enumerators();
    std::string result;
    bool first = true;
    for (const auto &p : es) {
      if (!first)
        result += ',';
      result += p.second;
      first = false;
    }
    return result;
  }
} const static enum_type_enumerators_reflected_enum_lv{};

// enum: omni::reflected_t<T>::enumerators()
struct enum_type_enumerators_reflected_t_t {
  template <typename T>
  std::string operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";

    auto es = omni::reflected_t<T>::enumerators();
    std::string result;
    bool first = true;
    for (const auto &p : es) {
      if (!first)
        result += ',';
      result += p.second;
      first = false;
    }
    return result;
  }
} const static enum_type_enumerators_reflected_t{};

// enum: omni::reflected<T>().enumerators()
struct enum_type_enumerators_reflected_fn_t {
  template <typename T>
  std::string operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";

    auto es = omni::reflected<T>().enumerators();
    std::string result;
    bool first = true;
    for (const auto &p : es) {
      if (!first)
        result += ',';
      result += p.second;
      first = false;
    }
    return result;
  }
} const static enum_type_enumerators_reflected_fn{};

// enum: omni::reflected(e).enumerators()
struct enum_type_enumerators_reflected_lv_t {
  template <typename T>
  std::string operator()(T &&e) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";

    auto es = omni::reflected(std::forward<T>(e)).enumerators();
    std::string result;
    bool first = true;
    for (const auto &p : es) {
      if (!first)
        result += ',';
      result += p.second;
      first = false;
    }
    return result;
  }
} const static enum_type_enumerators_reflected_lv{};

} // namespace enumerators

namespace fields {

// `auto` in labmda support only since C++14
// visit meta::public_fields() -> std::vector<std::string> of field names
struct fields_visitor {
  template <typename... Field>
  std::vector<std::string> operator()(const Field &...) const {
    return std::vector<std::string>{Field::name()...};
  }
};

struct field_type_names_visitor {
  template <typename... Field>
  std::vector<std::string> operator()(const Field &...) const {
    return std::vector<std::string>{Field::type_name()...};
  }
};

// record: omni::reflected_record_t<T>::public_fields()
struct record_type_fields_reflected_record_t_t {
  template <typename T>
  std::vector<std::string> operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();
    return omni::compat::apply(fields_visitor{},
      omni::reflected_record_t<T>::public_fields());
  }
} const static record_type_fields_reflected_record_t{};

// record: omni::reflected_record<T>().public_fields()
struct record_type_fields_reflected_record_fn_t {
  template <typename T>
  std::vector<std::string> operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();
    return omni::compat::apply(fields_visitor{},
      omni::reflected_record<T>().public_fields());
  }
} const static record_type_fields_reflected_record_fn{};

// record: omni::reflected_t<T>::public_fields()
struct record_type_fields_reflected_t_t {
  template <typename T>
  std::vector<std::string> operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();
    return omni::compat::apply(fields_visitor{},
      omni::reflected_t<T>::public_fields());
  }
} const static record_type_fields_reflected_t{};

// record: omni::reflected<T>().public_fields()
struct record_type_fields_reflected_fn2_t {
  template <typename T>
  std::vector<std::string> operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();
    return omni::compat::apply(fields_visitor{}, omni::reflected<T>().public_fields());
  }
} const static record_type_fields_reflected_fn2{};

struct record_type_field_type_names_t {
  template <typename T>
  std::vector<std::string> operator()(const T &t) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();
    return omni::compat::apply(field_type_names_visitor{},
      omni::reflected(t).public_fields());
  }
} const static record_type_field_type_names{};

} // namespace fields

namespace field_value_read {

struct field_values_visitor {
  template <typename V>
  static typename std::enable_if<std::is_integral<V>::value, std::string>::type
    to_string_value(const V &v) {
    return std::to_string(v);
  }

  static std::string to_string_value(const std::string &v) {
    return v;
  }

  template <typename... Binding>
  std::vector<std::string> operator()(Binding... b) const {
    return std::vector<std::string>{to_string_value(b.value())...};
  }
};

// record: omni::reflected_record_t<T>::public_fields(t)
struct record_type_field_values_reflected_record_t_t {
  template <typename T>
  std::vector<std::string> operator()(T &t) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();
    return omni::compat::apply(field_values_visitor{},
      omni::reflected_record_t<T>::public_fields(t));
  }
} const static record_type_field_values_reflected_record_t{};

// record: omni::reflected_record<T>().public_fields(t)
struct record_type_field_values_reflected_record_fn_t {
  template <typename T>
  std::vector<std::string> operator()(T &t) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();
    return omni::compat::apply(field_values_visitor{},
      omni::reflected_record<T>().public_fields(t));
  }
} const static record_type_field_values_reflected_record_fn{};

// record: omni::reflected_record(t).public_fields()  (non-owning binding)
struct record_type_field_values_reflected_record_lv_t {
  template <typename T>
  std::vector<std::string> operator()(T &t) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();

    auto binding = omni::reflected_record(t);
    return omni::compat::apply(field_values_visitor{}, binding.public_fields());
  }
} const static record_type_field_values_reflected_record_lv{};

// record: omni::reflected_t<T>::public_fields(t)
struct record_type_field_values_reflected_t_t {
  template <typename T>
  std::vector<std::string> operator()(T &t) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();
    return omni::compat::apply(field_values_visitor{},
      omni::reflected_t<T>::public_fields(t));
  }
} const static record_type_field_values_reflected_t{};

// record: omni::reflected<T>().public_fields(t)
struct record_type_field_values_reflected_fn_t2 {
  template <typename T>
  std::vector<std::string> operator()(T &t) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();
    return omni::compat::apply(field_values_visitor{},
      omni::reflected<T>().public_fields(t));
  }
} const static record_type_field_values_reflected_fn2{};

// record: omni::reflected(t).public_fields()  (non-owning binding)
struct record_type_field_values_reflected_lv_t2 {
  template <typename T>
  std::vector<std::string> operator()(T &t) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();

    auto binding = omni::reflected(t);
    return omni::compat::apply(field_values_visitor{}, binding.public_fields());
  }
} const static record_type_field_values_reflected_lv2{};

// record: omni::reflected_record(T(t)).public_fields()  (owning binding)
struct record_type_field_values_reflected_record_own_t {
  template <typename T>
  std::vector<std::string> operator()(T t) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();

    auto owning_binding = omni::reflected_record(std::move(t));
    return omni::compat::apply(field_values_visitor{}, owning_binding.public_fields());
  }
} const static record_type_field_values_reflected_record_own{};

// record: omni::reflected(T(t)).public_fields()  (owning binding, polymorphic)
struct record_type_field_values_reflected_own_t {
  template <typename T>
  std::vector<std::string> operator()(T t) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();

    auto owning_binding = omni::reflected(std::move(t));
    return omni::compat::apply(field_values_visitor{}, owning_binding.public_fields());
  }
} const static record_type_field_values_reflected_own{};

} // namespace field_value_read

namespace field_value_write {

struct assign_fields {
  record_type_t expected;

  template <typename... FieldBinding>
  void operator()(FieldBinding... b) const {
    int dummy[] = {0, (assign_one(b), 0)...};
    (void)dummy;
  }

  private:
  // int field
  template <typename FieldBinding>
  typename std::enable_if<
    std::is_same<int,
      typename std::decay<typename FieldBinding::type>::type>::value,
    void>::type
    assign_one(FieldBinding b) const {
    b.set_value(expected.first);
  }

  // std::string field
  template <typename FieldBinding>
  typename std::enable_if<
    std::is_same<std::string,
      typename std::decay<typename FieldBinding::type>::type>::value,
    void>::type
    assign_one(FieldBinding b) const {
    b.set_value(expected.second);
  }
};

// record: omni::reflected_record_t<T>::public_fields(t)  (non-owning)
struct record_type_field_write_reflected_record_t_t {
  template <typename T>
  void operator()(T &t, const record_type_t &expected) const {
    if (!omni::is_reflected<T>::value)
      return;

    omni::compat::apply(assign_fields{expected},
      omni::reflected_record_t<T>::public_fields(t));
  }
} const static record_type_field_write_reflected_record_t{};

// record: omni::reflected_record<T>().public_fields(t)  (non-owning)
struct record_type_field_write_reflected_record_fn_t {
  template <typename T>
  void operator()(T &t, const record_type_t &expected) const {
    if (!omni::is_reflected<T>::value)
      return;

    omni::compat::apply(assign_fields{expected},
      omni::reflected_record<T>().public_fields(t));
  }
} const static record_type_field_write_reflected_record_fn{};

// record: omni::reflected_record(t).public_fields()  (non-owning binding)
struct record_type_field_write_reflected_record_lv_t {
  template <typename T>
  void operator()(T &t, const record_type_t &expected) const {
    if (!omni::is_reflected<T>::value)
      return;

    auto binding = omni::reflected_record(t);
    omni::compat::apply(assign_fields{expected}, binding.public_fields());
  }
} const static record_type_field_write_reflected_record_lv{};

// record: omni::reflected_t<T>::public_fields(t)  (non-owning)
struct record_type_field_write_reflected_t_t {
  template <typename T>
  void operator()(T &t, const record_type_t &expected) const {
    if (!omni::is_reflected<T>::value)
      return;

    omni::compat::apply(assign_fields{expected},
      omni::reflected_t<T>::public_fields(t));
  }
} const static record_type_field_write_reflected_t{};

// record: omni::reflected<T>().public_fields(t)  (non-owning)
struct record_type_field_write_reflected_fn_t2 {
  template <typename T>
  void operator()(T &t, const record_type_t &expected) const {
    if (!omni::is_reflected<T>::value)
      return;

    omni::compat::apply(assign_fields{expected},
      omni::reflected<T>().public_fields(t));
  }
} const static record_type_field_write_reflected_fn2{};

// record: omni::reflected(t).public_fields()  (non-owning binding)
struct record_type_field_write_reflected_lv_t2 {
  template <typename T>
  void operator()(T &t, const record_type_t &expected) const {
    if (!omni::is_reflected<T>::value)
      return;

    auto binding = omni::reflected(t);
    omni::compat::apply(assign_fields{expected}, binding.public_fields());
  }
} const static record_type_field_write_reflected_lv2{};

// record: omni::reflected_record(T{...}).public_fields()  (owning binding)
struct record_type_field_write_reflected_record_own_t {
  template <typename T>
  T operator()(T t, const record_type_t &expected) const {
    if (!omni::is_reflected<T>::value)
      return t;

    auto owning_binding = omni::reflected_record(std::move(t));
    omni::compat::apply(assign_fields{expected}, owning_binding.public_fields());
    return std::move(owning_binding.bound);
  }
} const static record_type_field_write_reflected_record_own{};

// record: omni::reflected(T{...}).public_fields()  (owning binding, polymorphic)
struct record_type_field_write_reflected_own_t {
  template <typename T>
  T operator()(T t, const record_type_t &expected) const {
    if (!omni::is_reflected<T>::value)
      return t;

    auto owning_binding = omni::reflected(std::move(t));
    omni::compat::apply(assign_fields{expected}, owning_binding.public_fields());
    return std::move(owning_binding.bound);
  }
} const static record_type_field_write_reflected_own{};

} // namespace field_value_write

// todo: implement
namespace fields_for_loop {} // namespace fields_for_loop

} // namespace interface_test
