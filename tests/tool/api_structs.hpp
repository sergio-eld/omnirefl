#pragma once

#include <cstdint>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <omnirefl/reflection.hpp>

namespace interface_test {

struct record_type_t {
  int first;
  std::string second;
};

struct field_qualification_record_t {
  int normal;
  const int constant;
  mutable int cache;
  unsigned flags : 3;
};

struct volatile_field_record_t {
  int normal;
  mutable volatile int cache;
  volatile int observed;
  volatile unsigned flags : 3;
};

struct parent_record_t {
  struct nested_record_t {
    int value;
  };
};

enum class enum_type_t {
  zero,
  one,
};

namespace nested {

struct namespaced_record_t {
  int value;
};

struct parent_record_t {
  struct nested_record_t {
    int value;
  };
};

enum class namespaced_enum_t {
  first,
  second,
};

struct namespaced_field_types_t {
  namespaced_record_t record;
  namespaced_enum_t state;
  parent_record_t::nested_record_t nested;
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
  template <typename _M>
  std::string operator()(omni::meta_t<_M> meta) const {
    return meta.type_name();
  }

  template <typename T>
  std::string operator()(omni::binding_t<T> binding) const {
    return binding.type_name();
  }
};

type_name_t const static type_name{};

struct qualified_type_name_t {
  template <typename _M>
  std::string operator()(omni::meta_t<_M> meta) const {
    return meta.qualified_type_name();
  }

  template <typename T>
  std::string operator()(omni::binding_t<T> binding) const {
    return binding.qualified_type_name();
  }
};

qualified_type_name_t const static qualified_type_name{};

namespace enumerators {

struct enum_type_enumerators_t {
  template <typename T>
  std::string operator()(omni::enum_binding_t<T> binding) const {
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

namespace bindings {

template <typename Validate>
struct conversion_qualifiers_t {
  Validate validate;

  template <typename... Binding>
  auto operator()(Binding &&...binding) const
    -> decltype(validate(std::forward<Binding>(binding)...)) {
    return validate(std::forward<Binding>(binding)...);
  }
};

} // namespace bindings

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

struct qualified_field_type_names_visitor {
  template <typename... Field>
  std::vector<std::string> operator()(const Field &...) const {
    return std::vector<std::string>{Field::qualified_type_name()...};
  }
};

struct record_type_fields_t {
  template <typename T>
  std::vector<std::string> operator()(omni::record_binding_t<T> binding) const {
    return omni::compat::apply(fields_visitor{}, binding.public_fields());
  }
};

record_type_fields_t const static record_type_fields{};

struct record_type_field_type_names_t {
  template <typename T>
  std::vector<std::string> operator()(omni::record_binding_t<T> binding) const {
    return omni::compat::apply(field_type_names_visitor{},
      binding.public_fields());
  }
} const static record_type_field_type_names{};

struct record_type_field_qualified_type_names_t {
  template <typename T>
  std::vector<std::string> operator()(omni::record_binding_t<T> binding) const {
    return omni::compat::apply(qualified_field_type_names_visitor{},
      binding.public_fields());
  }
} const static record_type_field_qualified_type_names{};

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
  std::vector<std::string> operator()(omni::record_binding_t<T> binding) const {
    return omni::compat::apply(field_values_visitor{}, binding.public_fields());
  }
};

record_type_field_values_t const static record_type_field_values{};
record_type_field_values_t const static record_type_field_values_rvalue{};

} // namespace field_value_read

namespace field_qualification {

template <typename Field, typename Owner, typename V>
struct can_set_meta {
  template <typename F>
  static auto test(int)
    -> decltype(F::set_value(std::declval<Owner>(), std::declval<V>()),
      std::true_type{});

  template <typename>
  static std::false_type test(...);

  static const bool value = decltype(test<Field>(0))::value;
};

template <typename Binding, typename V>
struct can_set_binding {
  template <typename B>
  static auto test(int)
    -> decltype(std::declval<B &>().set_value(std::declval<V>()),
      std::true_type{});

  template <typename>
  static std::false_type test(...);

  static const bool value = decltype(test<Binding>(0))::value;
};

inline std::string bool_name(bool value) {
  return value ? "true" : "false";
}

template <typename Field>
std::string flags_for() {
  return std::string{Field::name()} //
  + ":const=" + bool_name(Field::is_const()) //
    + ":mutable=" + bool_name(Field::is_mutable()) //
    + ":volatile=" + bool_name(Field::is_volatile());
}

template <typename Field>
std::string meta_write_for() {
  return std::string{Field::name()} //
  + ":mutable_owner="
    + bool_name(can_set_meta<Field, field_qualification_record_t &, int>::value)
    + ":const_owner="
    + bool_name(
      can_set_meta<Field, const field_qualification_record_t &, int>::value);
}

template <typename Binding>
std::string binding_write_for() {
  return std::string{Binding::name()} //
  + ":set_value=" + bool_name(can_set_binding<Binding, int>::value);
}

struct field_flags_visitor {
  template <typename... Field>
  std::vector<std::string> operator()(Field...) const {
    return std::vector<std::string>{flags_for<Field>()...};
  }
};

struct meta_write_visitor {
  template <typename... Field>
  std::vector<std::string> operator()(Field...) const {
    return std::vector<std::string>{meta_write_for<Field>()...};
  }
};

struct binding_write_visitor {
  template <typename... Binding>
  std::vector<std::string> operator()(Binding...) const {
    return std::vector<std::string>{binding_write_for<Binding>()...};
  }
};

struct additional_volatile_forms_t {
  template <typename T>
  int operator()(
    omni::record_binding_t<const volatile T &> const_volatile_binding,
    omni::record_binding_t<T &> mutable_binding) const {
    auto const_volatile_fields = const_volatile_binding.public_fields();
    auto mutable_fields = mutable_binding.public_fields();
    typedef decltype(std::get<0>(const_volatile_fields)
        .value()) const_volatile_reference;
    typedef decltype(std::get<1>(const_volatile_fields)
        .value()) mutable_volatile_reference;
    typedef decltype(std::get<2>(mutable_fields).value()) volatile_reference;
    typedef
      typename std::tuple_element<1, decltype(const_volatile_fields)>::type
        mutable_volatile_field;
    typedef typename std::tuple_element<3, decltype(mutable_fields)>::type
      volatile_bitfield;
    typedef decltype(std::get<3>(mutable_fields)
        .value()) volatile_bitfield_value;

    static_assert(
      std::is_same<const volatile int &, const_volatile_reference>::value,
      "ordinary fields must preserve const-volatile owner qualification");
    static_assert(
      std::is_same<volatile int &, mutable_volatile_reference>::value,
      "mutable fields must drop const and preserve volatile qualification");
    static_assert(std::is_same<const volatile int &, volatile_reference>::value,
      "volatile fields must preserve their declared qualification");
    static_assert(mutable_volatile_field::is_mutable(),
      "mutable-volatile fields must remain mutable");
    static_assert(mutable_volatile_field::is_volatile(),
      "mutable-volatile fields must remain volatile");
    static_assert(volatile_bitfield::is_volatile(),
      "volatile bitfields must retain volatile metadata");
    static_assert(std::is_same<unsigned, volatile_bitfield_value>::value,
      "copied volatile bitfields must drop top-level qualification");

    std::get<1>(const_volatile_fields).set_value(17);
    std::get<2>(mutable_fields).set_value(29);
    std::get<3>(mutable_fields).set_value(5);
    return std::get<1>(const_volatile_fields).value()
      + std::get<2>(mutable_fields).value()
      + std::get<3>(mutable_fields).value();
  }
};

additional_volatile_forms_t const static additional_volatile_forms{};

struct field_flags_from_meta_t {
  template <typename _M>
  std::vector<std::string> operator()(omni::record_meta_t<_M> meta) const {
    return omni::compat::apply(field_flags_visitor{}, meta.public_fields());
  }
};

struct field_flags_from_binding_t {
  template <typename T>
  std::vector<std::string> operator()(omni::record_binding_t<T> binding) const {
    return omni::compat::apply(field_flags_visitor{}, binding.public_fields());
  }
};

struct meta_write_availability_t {
  template <typename _M>
  std::vector<std::string> operator()(omni::record_meta_t<_M> meta) const {
    return omni::compat::apply(meta_write_visitor{}, meta.public_fields());
  }
};

struct binding_write_availability_t {
  template <typename T>
  std::vector<std::string> operator()(omni::record_binding_t<T> binding) const {
    return omni::compat::apply(binding_write_visitor{},
      binding.public_fields());
  }
};

field_flags_from_meta_t const static field_flags_from_meta{};
field_flags_from_binding_t const static field_flags_from_binding{};
meta_write_availability_t const static meta_write_availability{};
binding_write_availability_t const static binding_write_availability{};

} // namespace field_qualification

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
      omni::compat::decay_t<typename FieldBinding::type>>::value,
    void>::type
    assign_one(FieldBinding b) const {
    b.set_value(expected.first);
  }

  // std::string field
  template <typename FieldBinding>
  typename std::enable_if<
    std::is_same<std::string,
      omni::compat::decay_t<typename FieldBinding::type>>::value,
    void>::type
    assign_one(FieldBinding b) const {
    b.set_value(expected.second);
  }
};

struct record_type_field_write_t {
  template <typename T>
  void operator()(omni::record_binding_t<T &> binding,
    const record_type_t &expected) const {
    omni::compat::apply(assign_fields{expected}, binding.public_fields());
  }
};

struct record_type_field_write_and_return_t {
  template <typename T>
  omni::compat::decay_t<T> operator()(omni::record_binding_t<T> binding,
    const record_type_t &expected) const {
    omni::compat::apply(assign_fields{expected}, binding.public_fields());
    return std::move(binding.record);
  }
};

record_type_field_write_t const static record_type_field_write{};
record_type_field_write_and_return_t const static record_type_field_write_and_return{};

struct record_type_field_write_call_t {
  record_type_t expected;

  template <typename T>
  void operator()(omni::record_binding_t<T &> binding) const {
    record_type_field_write(binding, expected);
  }
};

struct record_type_field_write_and_return_call_t {
  record_type_t expected;

  template <typename T>
  omni::compat::decay_t<T> operator()(omni::record_binding_t<T> binding) const {
    return record_type_field_write_and_return(binding, expected);
  }
};

} // namespace field_value_write

namespace inline_examples {

struct rvalue_binding_result_t {
  std::vector<std::string> fields;
  bool owns_value;
};

struct rvalue_binding_can_be_named_t {
  template <typename T>
  rvalue_binding_result_t operator()(omni::record_binding_t<T>) const {
    using value_type = omni::compat::decay_t<T>;
    auto value = omni::reflected(value_type{815, "oceanic"});
    return {
      field_value_read::record_type_field_values(value),
      decltype(value)::owning::value,
    };
  }
};

rvalue_binding_can_be_named_t const static rvalue_binding_can_be_named{};

} // namespace inline_examples

// todo: implement
namespace fields_for_loop {} // namespace fields_for_loop

} // namespace interface_test
