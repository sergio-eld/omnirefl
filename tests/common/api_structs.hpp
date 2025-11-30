#pragma once

#include <string>
#include <vector>

#include <omnirefl/reflected_scope.hpp>

#if __cplusplus >= 201703L
namespace compat {
using std::apply;
}
#else
namespace test_support {
using omni::detail::apply;
}
#endif

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
    return compat::apply(fields_visitor{},
      omni::reflected_tagged_t<T>::fields());
  }
} const static tagged_type_fields_reflected_tagged_t{};

// tagged: omni::reflected_tagged<T>().fields()
struct tagged_type_fields_reflected_tagged_fn_t {
  template <typename T>
  std::vector<std::string> operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();
    return compat::apply(fields_visitor{},
      omni::reflected_tagged<T>().fields());
  }
} const static tagged_type_fields_reflected_tagged_fn{};

// tagged: omni::reflected_t<T>::fields()
struct tagged_type_fields_reflected_t_t {
  template <typename T>
  std::vector<std::string> operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();
    return compat::apply(fields_visitor{}, omni::reflected_t<T>::fields());
  }
} const static tagged_type_fields_reflected_t{};

// tagged: omni::reflected<T>().fields()
struct tagged_type_fields_reflected_fn2_t {
  template <typename T>
  std::vector<std::string> operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();
    return compat::apply(fields_visitor{}, omni::reflected<T>().fields());
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
    return compat::apply(field_values_visitor{},
      omni::reflected_tagged_t<T>::fields(t));
  }
} const static tagged_type_field_values_reflected_tagged_t{};

// tagged: omni::reflected_tagged<T>().fields(t)
struct tagged_type_field_values_reflected_tagged_fn_t {
  template <typename T>
  std::vector<std::string> operator()(T &t) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();
    return compat::apply(field_values_visitor{},
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
    return compat::apply(field_values_visitor{}, binding.fields());
  }
} const static tagged_type_field_values_reflected_tagged_lv{};

// tagged: omni::reflected_t<T>::fields(t)
struct tagged_type_field_values_reflected_t_t {
  template <typename T>
  std::vector<std::string> operator()(T &t) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();
    return compat::apply(field_values_visitor{},
      omni::reflected_t<T>::fields(t));
  }
} const static tagged_type_field_values_reflected_t{};

// tagged: omni::reflected<T>().fields(t)
struct tagged_type_field_values_reflected_fn_t2 {
  template <typename T>
  std::vector<std::string> operator()(T &t) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();
    return compat::apply(field_values_visitor{},
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
    return compat::apply(field_values_visitor{}, binding.fields());
  }
} const static tagged_type_field_values_reflected_lv2{};

// tagged: omni::reflected_tagged(T(t)).fields()  (owning binding)
struct tagged_type_field_values_reflected_tagged_own_t {
  template <typename T>
  std::vector<std::string> operator()(T t) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();

    auto owning_binding = omni::reflected_tagged(std::move(t));
    return compat::apply(field_values_visitor{}, owning_binding.fields());
  }
} const static tagged_type_field_values_reflected_tagged_own{};

// tagged: omni::reflected(T(t)).fields()  (owning binding, polymorphic)
struct tagged_type_field_values_reflected_own_t {
  template <typename T>
  std::vector<std::string> operator()(T t) const {
    if (!omni::is_reflected<T>::value)
      return std::vector<std::string>();

    auto owning_binding = omni::reflected(std::move(t));
    return compat::apply(field_values_visitor{}, owning_binding.fields());
  }
} const static tagged_type_field_values_reflected_own{};

} // namespace field_value_read

} // namespace interface_test
