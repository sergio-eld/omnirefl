#pragma once

#include <string>
#include <vector>

#include <omnirefl/reflected_scope.hpp>

namespace interface_test {

struct tagged_type_t {
  int first;
  std::string second;
};

enum class enum_type_t {
  zero,
  one,
};

namespace type_identity {

// tagged: omni::reflected_tagged_t<T>
struct tagged_type_name_reflected_tagged_t_t {
  template <typename T>
  std::string operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected_tagged_t<T>::name();
  }
} const static tagged_type_name_reflected_tagged_t{};

// tagged: omni::reflected_tagged<T>()
struct tagged_type_name_reflected_tagged_fn_t {
  template <typename T>
  std::string operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected_tagged<T>().name();
  }
} const static tagged_type_name_reflected_tagged_fn{};

// tagged: omni::reflected_tagged(t)
struct tagged_type_name_reflected_tagged_lv_t {
  template <typename T>
  std::string operator()(T &&t) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected_tagged(std::forward<T>(t)).name();
  }
} const static tagged_type_name_reflected_tagged_lv{};

// tagged: omni::reflected_t<T>
struct tagged_type_name_reflected_t_t {
  template <typename T>
  std::string operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected_t<T>::name();
  }
} const static tagged_type_name_reflected_t{};

// tagged: omni::reflected<T>()
struct tagged_type_name_reflected_fn_t {
  template <typename T>
  std::string operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected<T>().name();
  }
} const static tagged_type_name_reflected_fn{};

// tagged: omni::reflected(t)
struct tagged_type_name_reflected_lv_t {
  template <typename T>
  std::string operator()(T &&t) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected(std::forward<T>(t)).name();
  }
} const static tagged_type_name_reflected_lv{};

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
// visit meta::fields() -> std::vector<std::string> of field names
struct fields_visitor {
  template <typename... Field>
  std::vector<std::string> operator()(const Field &...) const {
    return std::vector<std::string>{Field::name()...};
  }
};

// tagged: omni::reflected_tagged_t<T>::fields()
struct tagged_type_fields_reflected_tagged_t_t {
  template <typename T>
  std::vector<std::string> operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();
    return omni::compat::apply(fields_visitor{},
      omni::reflected_tagged_t<T>::fields());
  }
} const static tagged_type_fields_reflected_tagged_t{};

// tagged: omni::reflected_tagged<T>().fields()
struct tagged_type_fields_reflected_tagged_fn_t {
  template <typename T>
  std::vector<std::string> operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();
    return omni::compat::apply(fields_visitor{},
      omni::reflected_tagged<T>().fields());
  }
} const static tagged_type_fields_reflected_tagged_fn{};

// tagged: omni::reflected_t<T>::fields()
struct tagged_type_fields_reflected_t_t {
  template <typename T>
  std::vector<std::string> operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();
    return omni::compat::apply(fields_visitor{},
      omni::reflected_t<T>::fields());
  }
} const static tagged_type_fields_reflected_t{};

// tagged: omni::reflected<T>().fields()
struct tagged_type_fields_reflected_fn2_t {
  template <typename T>
  std::vector<std::string> operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();
    return omni::compat::apply(fields_visitor{}, omni::reflected<T>().fields());
  }
} const static tagged_type_fields_reflected_fn2{};

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

// tagged: omni::reflected_tagged_t<T>::fields(t)
struct tagged_type_field_values_reflected_tagged_t_t {
  template <typename T>
  std::vector<std::string> operator()(T &t) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();
    return omni::compat::apply(field_values_visitor{},
      omni::reflected_tagged_t<T>::fields(t));
  }
} const static tagged_type_field_values_reflected_tagged_t{};

// tagged: omni::reflected_tagged<T>().fields(t)
struct tagged_type_field_values_reflected_tagged_fn_t {
  template <typename T>
  std::vector<std::string> operator()(T &t) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();
    return omni::compat::apply(field_values_visitor{},
      omni::reflected_tagged<T>().fields(t));
  }
} const static tagged_type_field_values_reflected_tagged_fn{};

// tagged: omni::reflected_tagged(t).fields()  (non-owning binding)
struct tagged_type_field_values_reflected_tagged_lv_t {
  template <typename T>
  std::vector<std::string> operator()(T &t) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();

    auto binding = omni::reflected_tagged(t);
    return omni::compat::apply(field_values_visitor{}, binding.fields());
  }
} const static tagged_type_field_values_reflected_tagged_lv{};

// tagged: omni::reflected_t<T>::fields(t)
struct tagged_type_field_values_reflected_t_t {
  template <typename T>
  std::vector<std::string> operator()(T &t) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();
    return omni::compat::apply(field_values_visitor{},
      omni::reflected_t<T>::fields(t));
  }
} const static tagged_type_field_values_reflected_t{};

// tagged: omni::reflected<T>().fields(t)
struct tagged_type_field_values_reflected_fn_t2 {
  template <typename T>
  std::vector<std::string> operator()(T &t) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();
    return omni::compat::apply(field_values_visitor{},
      omni::reflected<T>().fields(t));
  }
} const static tagged_type_field_values_reflected_fn2{};

// tagged: omni::reflected(t).fields()  (non-owning binding)
struct tagged_type_field_values_reflected_lv_t2 {
  template <typename T>
  std::vector<std::string> operator()(T &t) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();

    auto binding = omni::reflected(t);
    return omni::compat::apply(field_values_visitor{}, binding.fields());
  }
} const static tagged_type_field_values_reflected_lv2{};

// tagged: omni::reflected_tagged(T(t)).fields()  (owning binding)
struct tagged_type_field_values_reflected_tagged_own_t {
  template <typename T>
  std::vector<std::string> operator()(T t) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();

    auto owning_binding = omni::reflected_tagged(std::move(t));
    return omni::compat::apply(field_values_visitor{}, owning_binding.fields());
  }
} const static tagged_type_field_values_reflected_tagged_own{};

// tagged: omni::reflected(T(t)).fields()  (owning binding, polymorphic)
struct tagged_type_field_values_reflected_own_t {
  template <typename T>
  std::vector<std::string> operator()(T t) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();

    auto owning_binding = omni::reflected(std::move(t));
    return omni::compat::apply(field_values_visitor{}, owning_binding.fields());
  }
} const static tagged_type_field_values_reflected_own{};

} // namespace field_value_read

namespace field_value_write {

struct assign_fields {
  tagged_type_t expected;

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

// tagged: omni::reflected_tagged_t<T>::fields(t)  (non-owning)
struct tagged_type_field_write_reflected_tagged_t_t {
  template <typename T>
  void operator()(T &t, const tagged_type_t &expected) const {
    if (!omni::is_reflected<T>::value)
      return;

    omni::compat::apply(assign_fields{expected},
      omni::reflected_tagged_t<T>::fields(t));
  }
} const static tagged_type_field_write_reflected_tagged_t{};

// tagged: omni::reflected_tagged<T>().fields(t)  (non-owning)
struct tagged_type_field_write_reflected_tagged_fn_t {
  template <typename T>
  void operator()(T &t, const tagged_type_t &expected) const {
    if (!omni::is_reflected<T>::value)
      return;

    omni::compat::apply(assign_fields{expected},
      omni::reflected_tagged<T>().fields(t));
  }
} const static tagged_type_field_write_reflected_tagged_fn{};

// tagged: omni::reflected_tagged(t).fields()  (non-owning binding)
struct tagged_type_field_write_reflected_tagged_lv_t {
  template <typename T>
  void operator()(T &t, const tagged_type_t &expected) const {
    if (!omni::is_reflected<T>::value)
      return;

    auto binding = omni::reflected_tagged(t);
    omni::compat::apply(assign_fields{expected}, binding.fields());
  }
} const static tagged_type_field_write_reflected_tagged_lv{};

// tagged: omni::reflected_t<T>::fields(t)  (non-owning)
struct tagged_type_field_write_reflected_t_t {
  template <typename T>
  void operator()(T &t, const tagged_type_t &expected) const {
    if (!omni::is_reflected<T>::value)
      return;

    omni::compat::apply(assign_fields{expected},
      omni::reflected_t<T>::fields(t));
  }
} const static tagged_type_field_write_reflected_t{};

// tagged: omni::reflected<T>().fields(t)  (non-owning)
struct tagged_type_field_write_reflected_fn_t2 {
  template <typename T>
  void operator()(T &t, const tagged_type_t &expected) const {
    if (!omni::is_reflected<T>::value)
      return;

    omni::compat::apply(assign_fields{expected},
      omni::reflected<T>().fields(t));
  }
} const static tagged_type_field_write_reflected_fn2{};

// tagged: omni::reflected(t).fields()  (non-owning binding)
struct tagged_type_field_write_reflected_lv_t2 {
  template <typename T>
  void operator()(T &t, const tagged_type_t &expected) const {
    if (!omni::is_reflected<T>::value)
      return;

    auto binding = omni::reflected(t);
    omni::compat::apply(assign_fields{expected}, binding.fields());
  }
} const static tagged_type_field_write_reflected_lv2{};

// tagged: omni::reflected_tagged(T{...}).fields()  (owning binding)
struct tagged_type_field_write_reflected_tagged_own_t {
  template <typename T>
  T operator()(T t, const tagged_type_t &expected) const {
    if (!omni::is_reflected<T>::value)
      return t;

    auto owning_binding = omni::reflected_tagged(std::move(t));
    omni::compat::apply(assign_fields{expected}, owning_binding.fields());
    return std::move(owning_binding.bound);
  }
} const static tagged_type_field_write_reflected_tagged_own{};

// tagged: omni::reflected(T{...}).fields()  (owning binding, polymorphic)
struct tagged_type_field_write_reflected_own_t {
  template <typename T>
  T operator()(T t, const tagged_type_t &expected) const {
    if (!omni::is_reflected<T>::value)
      return t;

    auto owning_binding = omni::reflected(std::move(t));
    omni::compat::apply(assign_fields{expected}, owning_binding.fields());
    return std::move(owning_binding.bound);
  }
} const static tagged_type_field_write_reflected_own{};

} // namespace field_value_write

// todo: implement
namespace fields_for_loop {} // namespace fields_for_loop

} // namespace interface_test
