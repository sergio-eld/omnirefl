#pragma once

// todo: add compat::expected and compat::optional

#include <omnirefl/reflected_scope.hpp>

// dependency_types.h
#pragma once

#include <string>
#include <tuple>
#include <type_traits>

#if defined(__cpp_lib_variant)
#  include <variant>
#else
#  include <mpark/variant.hpp>
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

namespace as_field {

static const struct get_dependency_name_t {
  template <typename T>
  std::string operator()(const T &v) const {
    using parent_type =
      typename std::remove_cv<typename std::remove_reference<T>::type>::type;

    const auto fields = omni::reflected(v).fields();
    const auto f = std::get<0>(fields);
    using dep_type = typename decltype(f)::type;

    return std::string(omni::reflected<parent_type>().name())
      + "::" + std::string(omni::reflected<dep_type>().name()) + ":int";
  }
} get_dependency_name;

static const struct get_dependency_name_layer_2_t {
  template <typename T>
  std::string operator()(const T &v) const {
    using root_type =
      typename std::remove_cv<typename std::remove_reference<T>::type>::type;

    const auto outer_fields = omni::reflected(v).fields();
    const auto outer_f = std::get<0>(outer_fields);
    using intermediate_type = typename decltype(outer_f)::type;

    const auto inner = outer_f.value();
    const auto inner_fields = omni::reflected(inner).fields();
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
  template <typename T>
  std::string operator()(const T &) const {
    using parent_type =
      typename std::remove_cv<typename std::remove_reference<T>::type>::type;
    using dep_type = typename T::value_type;

    return std::string(omni::reflected<parent_type>().name())
      + "::" + std::string(omni::reflected<dep_type>().name()) + ":int";
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

    const auto fields = omni::reflected(v).fields();
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

    const auto outer_fields = omni::reflected(v).fields();
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

} // namespace resolved

// ---------- holders (dependency chains) ----------

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

// tuple / variant dependency chains

struct template_dep_level_1 {
  std::tuple<resolved::as_template_arg> tpl_field_1;
};

struct template_dep_level_2 {
  omni::compat::variant<std::tuple<resolved::as_template_arg_layer_2>>
    var_field_2;
};

} // namespace dependency_types
