#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <omnirefl/reflection.hpp>

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

struct type_name_t {
  template <typename T>
  std::string operator()(omni::meta_t<T> meta) const {
    return meta.name();
  }

  template <typename T>
  std::string operator()(omni::binding_t<T> binding) const {
    return binding.name();
  }
};

type_name_t const static type_name{};

namespace enumerators {

struct enum_type_enumerators_t {
  template <typename T>
  std::string operator()(omni::binding_t<T> binding) const {
    auto es = binding.enumerators();
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
};

enum_type_enumerators_t const static enum_type_enumerators{};

} // namespace enumerators

namespace fields {

// `auto` in lambda support only since C++14.
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

struct record_type_fields_t {
  template <typename T>
  std::vector<std::string> operator()(omni::binding_t<T> binding) const {
    return omni::compat::apply(fields_visitor{}, binding.public_fields());
  }
};

record_type_fields_t const static record_type_fields{};

struct record_type_field_type_names_t {
  template <typename T>
  std::vector<std::string> operator()(omni::binding_t<T> binding) const {
    return omni::compat::apply(field_type_names_visitor{},
      binding.public_fields());
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

struct record_type_field_values_t {
  template <typename T>
  std::vector<std::string> operator()(omni::binding_t<T> binding) const {
    return omni::compat::apply(field_values_visitor{},
      binding.public_fields());
  }
};

record_type_field_values_t const static record_type_field_values{};
record_type_field_values_t const static record_type_field_values_own{};

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

struct record_type_field_write_t {
  template <typename T>
  void operator()(omni::binding_t<T &> binding,
    const record_type_t &expected) const {
    omni::compat::apply(assign_fields{expected}, binding.public_fields());
  }
};

struct record_type_field_write_own_t {
  template <typename T>
  T operator()(omni::binding_t<T> binding,
    const record_type_t &expected) const {
    omni::compat::apply(assign_fields{expected}, binding.public_fields());
    return binding;
  }
};

record_type_field_write_t const static record_type_field_write{};
record_type_field_write_own_t const static record_type_field_write_own{};

struct record_type_field_write_call_t {
  record_type_t expected;

  template <typename T>
  void operator()(omni::binding_t<T &> binding) const {
    record_type_field_write(binding, expected);
  }
};

struct record_type_field_write_own_call_t {
  record_type_t expected;

  template <typename T>
  T operator()(omni::binding_t<T> binding) const {
    return record_type_field_write_own(binding, expected);
  }
};

} // namespace field_value_write

namespace inline_examples {

struct rvalue_binding_can_be_named_t {
  template <typename T>
  std::vector<std::string> operator()(omni::binding_t<T>) const {
    auto value = omni::reflected(T{815, "oceanic"});
    return field_value_read::record_type_field_values(value);
  }
};

rvalue_binding_can_be_named_t const static rvalue_binding_can_be_named{};

} // namespace inline_examples

// todo: implement
namespace fields_for_loop {} // namespace fields_for_loop

} // namespace interface_test
