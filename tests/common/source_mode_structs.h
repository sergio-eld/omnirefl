#pragma once

#include "inplace_structs.h"

#include <omnirefl/reflected_call.hpp>
#include <omnirefl/reflected_scope.hpp>

#include <mpark/variant.hpp>

#include <map>
#include <sstream>
#include <string>
#include <type_traits>
#if defined CXX_STANDARD && 17 <= CXX_STANDARD
#  include <variant>
#endif
#include <vector>

namespace source_mode_compat {
template <typename T, typename... V>
const T *get_if(const mpark::variant<V...> *value) {
  return mpark::get_if<T>(value);
}

#if defined CXX_STANDARD && 17 <= CXX_STANDARD
template <typename T, typename... V>
const T *get_if(const std::variant<V...> *value) {
  return std::get_if<T>(value);
}
#endif
} // namespace source_mode_compat

namespace source_mode {
struct record {
  std::string name;
  int count;
  double score;
};

struct scalar_pack {
  bool enabled;
  char code;
  unsigned level;
  std::string label;
};

struct writable_scalar_pack {
  char code;
  unsigned level;
  std::string label;
};

struct foo_bar_record {
  int foo_count;
  int bar_count;
  int untouched_count;
  std::string foo_name;
};

struct nested_record {
  int value;
  std::string tag;
};

struct nested_holder {
  nested_record nested;
  std::string name;
};

struct bitfield_record {
  unsigned flags : 3;
  unsigned code : 5;
  int count;
};

struct derived_from_header: example::in_header_struct {
  std::string derived_name;
  int derived_count;
  double derived_score;
};

struct derived_from_record: record {
  std::string derived_name;
  int derived_count;
  double derived_score;
};

struct multi_base: example::in_header_struct, record {
  std::string multi_name;
  int multi_count;
  double multi_score;
};

struct private_base: private record {
  std::string name;
  int count;
  double score;
};

struct protected_base: protected example::in_header_struct {
  std::string name;
  int count;
  double score;
};

class mixed_access {
  std::string hidden = "you can't see me";

  public:
  std::string name;
  int count = 0;
  double score = 0.0;

  const std::string &hidden_value() const {
    return hidden;
  }
};

enum class scoped_enum {
  alpha,
  beta,
  gamma,
};
} // namespace source_mode

namespace source_mode_impl {
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

struct field_names_t {
  struct _visit {
    template <typename... Field>
    std::vector<std::string> operator()(const Field &...field) const {
      return {std::string(field.name())...};
    }
  };

  template <typename T>
  std::vector<std::string> operator()(const T &value) const {
    const auto fields = omni::reflected(value).public_fields();
    return omni::compat::apply(_visit{}, fields);
  }
} const static field_names{};

struct enum_names_t {
  template <typename Enum>
  std::vector<std::string> operator()(Enum) const {
    const auto enums = omni::reflected_enum_t<Enum>::enumerators();
    std::vector<std::string> names;
    for (const auto &value_name : enums)
      names.emplace_back(value_name.second);
    return names;
  }
} const static enum_names{};

struct maybe_field_names_t {
  std::vector<std::string> operator()(int) const {
    return {};
  }

  std::vector<std::string> operator()(const std::string &) const {
    return {};
  }

  template <typename T>
  std::vector<std::string> operator()(const T &value) const {
    static_assert(omni::is_reflected<T>::value, "");
    return field_names(value);
  }
} const static maybe_field_names{};

struct field_values_t {
  template <typename T>
  std::vector<std::string> operator()(const T &value) const {
    const auto fields = omni::reflected(value).public_fields();
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
    std::is_same<bool, typename Field::type>::value>::type
    _append(std::vector<std::string> &out, const Field &field) {
    out.emplace_back(field.value() ? "true" : "false");
  }

  template <typename Field>
  static typename std::enable_if<
    std::is_same<char, typename Field::type>::value>::type
    _append(std::vector<std::string> &out, const Field &field) {
    out.emplace_back(1, field.value());
  }

  template <typename Field>
  static typename std::enable_if<
    !std::is_same<std::string, typename Field::type>::value
      && !std::is_same<int, typename Field::type>::value
      && !std::is_same<unsigned, typename Field::type>::value
      && !std::is_same<double, typename Field::type>::value
      && !std::is_same<bool, typename Field::type>::value
      && !std::is_same<char, typename Field::type>::value>::type
    _append(std::vector<std::string> &, const Field &) {}
} const static field_values{};

template <typename T, typename V>
void from_std_map(const std::map<std::string, V> &from, T &to);

template <typename T, typename V>
T from_std_map(const std::map<std::string, V> &from);

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

    const auto *value = source_mode_compat::get_if<
      typename Field::type>(&from.at(field.name()));
    if (value)
      field.set_value(*value);
  }

  template <typename V, typename Field>
  static typename std::enable_if<
    is_reflected_record<typename Field::type>::value>::type
    _write_field(const std::map<std::string, V> &from, Field field) {
    if (0 != from.count(field.name())
        && _write_field_from_nested_map(from.at(field.name()), field))
      return;

    field.set_value(from_std_map<typename Field::type>(from));
  }

  template <typename Field, typename Nested>
  static typename std::enable_if<is_string_map<Nested>::value, bool>::type
    _try_nested_map(const Nested *nested, Field field) {
    if (!nested)
      return false;

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
          || _try_nested_map(source_mode_compat::get_if<V>(&value),
            field),
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
          || _try_nested_map(source_mode_compat::get_if<V>(&value),
            field),
        0)...};
    (void)dummy;
    return written;
  }
#endif

  template <typename Field, typename V>
  static bool _write_field_from_nested_map(const V &, Field) {
    return false;
  }
};

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

template <typename T, typename V>
void from_std_map(const std::map<std::string, V> &from, T &to) {
  omni::reflected_call(from_std_map_adapter<V>{from}, to);
}

template <typename T, typename V>
T from_std_map(const std::map<std::string, V> &from) {
  return omni::reflected_call(from_std_map_return_adapter<T, V>{from}, T{});
}

struct write_foo_bar_t {
  template <typename T>
  void operator()(T &value) const {
    const auto fields = omni::reflected(value).public_fields();
    omni::compat::apply(*this, fields);
  }

  template <typename... Field>
  void operator()(Field... field) const {
    int dummy[] = {0, (_write_field(field), 0)...};
    (void)dummy;
  }

  private:
  template <typename Field>
  static typename std::enable_if<
    std::is_same<int, typename Field::type>::value>::type
    _write_field(Field field) {
    const std::string name = field.name();
    if (std::string::npos != name.find("foo"))
      field.set_value(8);
    if (std::string::npos != name.find("bar"))
      field.set_value(15);
  }

  template <typename Field>
  static typename std::enable_if<
    !std::is_same<int, typename Field::type>::value>::type
    _write_field(Field) {}
} const static write_foo_bar{};
} // namespace source_mode_impl
