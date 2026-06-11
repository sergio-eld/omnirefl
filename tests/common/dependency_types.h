#pragma once

#include <omnirefl/reflected_scope.hpp>

#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include <mpark/variant.hpp>

#if defined(__cpp_lib_variant)
#  include <variant>
#endif

namespace omni {
namespace compat {
#if defined(__cpp_lib_variant)
template <typename... T>
using variant = std::variant<T...>;
#else
template <typename... T>
using variant = mpark::variant<T...>;
#endif
} // namespace compat
} // namespace omni

namespace dependency_types {

// ---------- visitors ----------

namespace inspect {

static const struct field_names_t {
  struct collect {
    template <typename... Field>
    std::vector<std::string> operator()(const Field &...f) const {
      return std::vector<std::string>{f.name()...};
    }
  };

  template <typename T>
  std::vector<std::string> operator()(const T &v) const {
    return omni::compat::apply(collect{}, omni::reflected(v).public_fields());
  }
} field_names;

static const struct field_indices_t {
  struct collect {
    template <typename... Field>
    std::vector<std::size_t> operator()(const Field &...f) const {
      return std::vector<std::size_t>{f.index()...};
    }
  };

  template <typename T>
  std::vector<std::size_t> operator()(const T &v) const {
    return omni::compat::apply(collect{}, omni::reflected(v).public_fields());
  }
} field_indices;

static const struct field_count_t {
  template <typename T>
  std::size_t operator()(const T &) const {
    return std::tuple_size<
      typename omni::reflected_record_t<T>::public_fields_t>::value;
  }
} field_count;

static const struct reflected_name_t {
  template <typename T>
  std::string operator()(const T &) const {
    return omni::reflected<T>().name();
  }
} reflected_name;

static const struct first_field_type_name_t {
  template <typename T>
  std::string operator()(const T &v) const {
    const auto fields = omni::reflected(v).public_fields();
    const auto f = std::get<0>(fields);
    typedef typename decltype(f)::type field_type;

    return omni::reflected<field_type>().name();
  }
} first_field_type_name;

static const struct second_field_type_name_t {
  template <typename T>
  std::string operator()(const T &v) const {
    const auto fields = omni::reflected(v).public_fields();
    const auto f = std::get<1>(fields);
    typedef typename decltype(f)::type field_type;

    return omni::reflected<field_type>().name();
  }
} second_field_type_name;

static const struct enum_names_t {
  template <typename Enum>
  std::vector<std::string> operator()(const Enum &) const {
    const auto enumerators = omni::reflected<Enum>().enumerators();
    std::vector<std::string> names;
    names.reserve(enumerators.size());

    for (std::size_t i = 0; i < enumerators.size(); ++i)
      names.emplace_back(enumerators[i].second);

    return names;
  }
} enum_names;

} // namespace inspect

// refactorme: 'namespace visit' for every visitor
namespace as_field {

static const struct get_dependency_name_t {
  template <typename ParentType>
  std::string operator()(const ParentType &v) const {
    const auto f = std::get<0>(omni::reflected(v).public_fields());
    using field_type = typename decltype(f)::type;

    return std::string(omni::reflected<ParentType>().name())
      + "::" + std::string(omni::reflected<field_type>().name()) + ":int";
  }
} get_dependency_name;

static const struct get_dependency_name_layer_2_t {
  template <typename T>
  std::string operator()(const T &v) const {
    using root_type =
      typename std::remove_cv<typename std::remove_reference<T>::type>::type;

    const auto outer_fields = omni::reflected(v).public_fields();
    const auto outer_f = std::get<0>(outer_fields);
    using intermediate_type = typename decltype(outer_f)::type;

    const auto inner = outer_f.value();
    const auto inner_fields = omni::reflected(inner).public_fields();
    const auto inner_f = std::get<0>(inner_fields);
    using dep_type = typename decltype(inner_f)::type;

    return std::string(omni::reflected<root_type>().name())
      + "::" + std::string(omni::reflected<intermediate_type>().name())
      + "::" + std::string(omni::reflected<dep_type>().name()) + ":int";
  }
} get_dependency_name_layer_2;

} // namespace as_field

namespace as_alias {

static const struct get_dependency_name_t {
  template <typename Parent>
  std::string operator()(const Parent &) const {
    static_assert(omni::is_reflected<Parent>::value, "Parent is not reflected");
    static_assert(omni::is_reflected<typename Parent::value_type>::value,
      "Member alias is not reflected");

    return std::string(omni::reflected<Parent>().name()) + "::"
      + std::string(omni::reflected<typename Parent::value_type>().name())
      + ":int";
  }
} get_dependency_name;

static const struct get_dependency_name_layer_2_t {
  template <typename T>
  std::string operator()(const T &) const {
    using root_type =
      typename std::remove_cv<typename std::remove_reference<T>::type>::type;
    using level_1 = typename T::value_type;
    using dep_type = typename level_1::value_type;

    return std::string(omni::reflected<root_type>().name())
      + "::" + std::string(omni::reflected<level_1>().name())
      + "::" + std::string(omni::reflected<dep_type>().name()) + ":int";
  }
} get_dependency_name_layer_2;

} // namespace as_alias

namespace as_template_arg {

static const struct get_dependency_name_t {
  template <typename T>
  std::string operator()(const T &v) const {
    using parent_type =
      typename std::remove_cv<typename std::remove_reference<T>::type>::type;

    const auto fields = omni::reflected(v).public_fields();
    const auto f = std::get<0>(fields);
    using tuple_type = typename decltype(f)::type;
    using dep_type = typename std::tuple_element<0, tuple_type>::type;

    return std::string(omni::reflected<parent_type>().name())
      + "::" + std::string(omni::reflected<dep_type>().name()) + ":int";
  }
} get_dependency_name;

static const struct get_dependency_name_layer_2_t {
  template <typename Variant>
  struct first_variant_arg;

  template <template <typename...> class V, typename First, typename... Rest>
  struct first_variant_arg<V<First, Rest...>> {
    using type = First;
  };

  template <typename T>
  std::string operator()(const T &v) const {
    using root_type =
      typename std::remove_cv<typename std::remove_reference<T>::type>::type;

    const auto outer_fields = omni::reflected(v).public_fields();
    const auto outer_f = std::get<0>(outer_fields);
    using outer_field_type = typename decltype(outer_f)::type;

    using intermediate_type =
      typename first_variant_arg<outer_field_type>::type; // std::tuple<...>
    using dep_type = typename std::tuple_element<0, intermediate_type>::type;

    return std::string(omni::reflected<root_type>().name())
      + "::tuple::" // intermediate_type is a std::tuple<...>
      + std::string(omni::reflected<dep_type>().name()) + ":int";
  }
} get_dependency_name_layer_2;

} // namespace as_template_arg

namespace as_sequence_arg {

static const struct get_vector_value_name_t {
  template <typename T>
  std::string operator()(const T &v) const {
    const auto fields = omni::reflected(v).public_fields();
    const auto f = std::get<0>(fields);
    typedef typename decltype(f)::type vector_type;
    typedef typename vector_type::value_type value_type;

    static_assert(omni::is_reflected<value_type>::value,
      "Vector value type is not reflected");

    return std::string(omni::reflected<T>().name())
      + "::vector::" + omni::reflected<value_type>().name();
  }
} get_vector_value_name;

static const struct get_tuple_value_name_t {
  template <typename T>
  std::string operator()(const T &v) const {
    const auto fields = omni::reflected(v).public_fields();
    const auto f = std::get<0>(fields);
    typedef typename decltype(f)::type tuple_type;
    typedef typename std::tuple_element<0, tuple_type>::type value_type;

    static_assert(omni::is_reflected<value_type>::value,
      "Tuple value type is not reflected");

    return std::string(omni::reflected<T>().name())
      + "::tuple::" + omni::reflected<value_type>().name();
  }
} get_tuple_value_name;

static const struct get_tuple_second_value_name_t {
  template <typename T>
  std::string operator()(const T &v) const {
    const auto fields = omni::reflected(v).public_fields();
    const auto f = std::get<0>(fields);
    typedef typename decltype(f)::type tuple_type;
    typedef typename std::tuple_element<1, tuple_type>::type value_type;

    static_assert(omni::is_reflected<value_type>::value,
      "Tuple second value type is not reflected");

    return std::string(omni::reflected<T>().name())
      + "::tuple::" + omni::reflected<value_type>().name();
  }
} get_tuple_second_value_name;

static const struct get_variant_value_name_t {
  template <typename Variant>
  struct first_variant_arg;

  template <template <typename...> class V, typename First, typename... Rest>
  struct first_variant_arg<V<First, Rest...>> {
    typedef First type;
  };

  template <typename T>
  std::string operator()(const T &v) const {
    const auto fields = omni::reflected(v).public_fields();
    const auto f = std::get<0>(fields);
    typedef typename decltype(f)::type variant_type;
    typedef typename first_variant_arg<variant_type>::type value_type;

    static_assert(omni::is_reflected<value_type>::value,
      "Variant value type is not reflected");

    return std::string(omni::reflected<T>().name())
      + "::variant::" + omni::reflected<value_type>().name();
  }
} get_variant_value_name;

static const struct get_nested_vector_tuple_value_name_t {
  template <typename T>
  std::string operator()(const T &v) const {
    const auto fields = omni::reflected(v).public_fields();
    const auto f = std::get<0>(fields);
    typedef typename decltype(f)::type vector_type;
    typedef typename vector_type::value_type tuple_type;
    typedef typename std::tuple_element<0, tuple_type>::type value_type;

    static_assert(omni::is_reflected<value_type>::value,
      "Nested vector tuple value type is not reflected");

    return std::string(omni::reflected<T>().name())
      + "::vector::tuple::" + omni::reflected<value_type>().name();
  }
} get_nested_vector_tuple_value_name;

} // namespace as_sequence_arg

namespace as_inherited_struct {

template <typename Base>
struct is_base_reflected_t {
  template <typename Derived>
  bool operator()(const Derived &) const {
    static_assert(std::is_base_of<Base, Derived>::value,
      "Input is not Derived from Base");
    return omni::is_reflected<Base>::value;
  }
};

} // namespace as_inherited_struct

// ---------- resolved dependency types ----------

namespace resolved {

struct as_field {
  int value;
};

struct as_field_layer_2 {
  int value;
};

struct as_alias {
  int value;
};

struct as_alias_layer_2 {
  int value;
};

struct as_template_arg {
  int value;
};

struct as_template_arg_layer_2 {
  int value;
};

struct as_inherited_struct {
  int base_field;
};

struct as_second_base {
  std::string second_base_field;
};

struct as_third_base {
  double third_base_field;
};

struct as_sequence_vector {
  int value;
};

struct as_sequence_tuple {
  int value;
};

struct as_sequence_variant {
  int value;
};

struct as_non_public_base {
  int hidden_base_field;
};

} // namespace resolved

enum plain_status {
  plain_status_pending,
  plain_status_running,
  plain_status_done,
};

enum class scoped_status {
  idle,
  busy,
  blocked,
};

enum fixed_status : unsigned {
  fixed_status_low,
  fixed_status_medium,
  fixed_status_high,
};

enum class scoped_fixed_status : unsigned {
  low,
  medium,
  high,
};

// ---------- holders (dependency chains) ----------

struct example_value {
  std::string name;
  int count;
  double score;
};

// field → dependency
struct field_dep_level_1 {
  resolved::as_field field_level_1;
};

// field → field → dependency
struct field_mid_level_2 {
  resolved::as_field_layer_2 field_mid_level_2;
};

struct field_dep_level_2 {
  field_mid_level_2 field_level_2;
};

struct field_mid_level_3 {
  field_dep_level_2 field_mid_level_3;
};

struct field_dep_level_3 {
  field_mid_level_3 field_level_3;
};

struct field_dep_two_fields {
  resolved::as_field first_dependency;
  resolved::as_field_layer_2 second_dependency;
};

// alias → dependency
struct alias_dep_level_1 {
  using value_type = resolved::as_alias;
};

// alias → alias → dependency
struct alias_dep_mid_level_2 {
  using value_type = resolved::as_alias_layer_2;
};

struct alias_dep_level_2 {
  using value_type = alias_dep_mid_level_2;
};

struct alias_dep_mid_level_3 {
  using value_type = alias_dep_level_2;
};

struct alias_dep_level_3 {
  using value_type = alias_dep_mid_level_3;
};

// tuple / variant dependency chains

struct template_dep_level_1 {
  std::tuple<resolved::as_template_arg> tpl_field_1;
};

struct template_dep_level_2 {
  omni::compat::variant<std::tuple<resolved::as_template_arg_layer_2>>
    var_field_2;
};

// explicit test of third-party variant
struct mpark_template_dep_level_2 {
  mpark::variant<std::tuple<resolved::as_template_arg_layer_2>> var_field_2;
};

struct vector_dep_level_1 {
  std::vector<resolved::as_sequence_vector> vector_field;
};

struct tuple_dep_two_values {
  std::tuple<resolved::as_sequence_tuple, resolved::as_field> tuple_field;
};

struct variant_dep_level_1 {
  omni::compat::variant<resolved::as_sequence_variant, resolved::as_field>
    variant_field;
};

struct mpark_variant_dep_level_1 {
  mpark::variant<resolved::as_sequence_variant, resolved::as_field>
    variant_field;
};

struct nested_vector_tuple_dep {
  std::vector<std::tuple<resolved::as_template_arg_layer_2>> nested_field;
};

struct derived_struct: resolved::as_inherited_struct {
  double derived_field;

  // ad hoc for C++11
  derived_struct(int base, double derived)
      : resolved::as_inherited_struct{base}
      , derived_field(derived) {}
};

struct multi_base_derived:
    resolved::as_inherited_struct,
    resolved::as_second_base {
  int own_field;

  multi_base_derived(int base, std::string second, int own)
      : resolved::as_inherited_struct{base}
      , resolved::as_second_base{second}
      , own_field(own) {}
};

struct deep_mid_derived: resolved::as_inherited_struct {
  std::string mid_field;

  deep_mid_derived(int base, std::string mid)
      : resolved::as_inherited_struct{base}
      , mid_field(mid) {}
};

struct deep_derived: deep_mid_derived {
  double deep_field;

  deep_derived(int base, std::string mid, double deep)
      : deep_mid_derived(base, mid)
      , deep_field(deep) {}
};

struct three_base_derived:
    resolved::as_inherited_struct,
    resolved::as_second_base,
    resolved::as_third_base {
  std::string own_field;

  three_base_derived(int base,
    std::string second,
    double third,
    std::string own)
      : resolved::as_inherited_struct{base}
      , resolved::as_second_base{second}
      , resolved::as_third_base{third}
      , own_field(own) {}
};

struct enum_holder {
  plain_status plain;
  scoped_status scoped;
  fixed_status fixed;
};

struct mixed_dependency_holder {
  example_value value;
  field_dep_level_1 field_dependency;
  vector_dep_level_1 vector_dependency;
  enum_holder enum_dependency;
};

struct non_public_fields {
  std::string visible;

  private:
  int hidden_private;

  protected:
  double hidden_protected;

  public:
  non_public_fields(std::string visible,
    int hidden_private,
    double hidden_protected)
      : visible(visible)
      , hidden_private(hidden_private)
      , hidden_protected(hidden_protected) {}

  int hidden_private_value() const {
    return hidden_private;
  }
};

struct private_base_derived: private resolved::as_non_public_base {
  std::string own_field;

  private_base_derived(int hidden_base, std::string own)
      : resolved::as_non_public_base{hidden_base}
      , own_field(own) {}
};

struct protected_base_derived: protected resolved::as_non_public_base {
  std::string own_field;

  protected_base_derived(int hidden_base, std::string own)
      : resolved::as_non_public_base{hidden_base}
      , own_field(own) {}
};

struct mixed_access_derived: private resolved::as_non_public_base {
  std::string visible;

  private:
  int hidden_private;

  protected:
  double hidden_protected;

  public:
  mixed_access_derived(int hidden_base,
    std::string visible,
    int hidden_private,
    double hidden_protected)
      : resolved::as_non_public_base{hidden_base}
      , visible(visible)
      , hidden_private(hidden_private)
      , hidden_protected(hidden_protected) {}

  int hidden_private_value() const {
    return hidden_private;
  }
};

// todo: add compat::expected and compat::optional
// todo: nested types struct foo{ struct bar{}; enum baz{}; };

} // namespace dependency_types
