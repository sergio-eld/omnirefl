#pragma once

#include <omnirefl/reflected_scope.hpp>

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
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
    return omni::compat::apply(collect{}, v.public_fields());
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
    return omni::compat::apply(collect{}, v.public_fields());
  }
} field_indices;

static const struct field_count_t {
  template <typename T>
  std::size_t operator()(const T &v) const {
    return std::tuple_size<decltype(v.public_fields())>::value;
  }
} field_count;

static const struct reflected_name_t {
  template <typename T>
  std::string operator()(const T &v) const {
    return v.name();
  }
} reflected_name;

static const struct reflected_annotation_t {
  template <typename T>
  std::string operator()(const T &v) const {
    return v.annotation();
  }
} reflected_annotation;

static const struct field_annotations_t {
  struct collect {
    template <typename... Field>
    std::vector<std::string> operator()(const Field &...f) const {
      return std::vector<std::string>{f.annotation()...};
    }
  };

  template <typename T>
  std::vector<std::string> operator()(const T &v) const {
    return omni::compat::apply(collect{}, v.public_fields());
  }
} field_annotations;

static const struct constexpr_annotations_t {
  template <typename T>
  bool operator()(const T &v) const {
    typedef typename std::tuple_element<
      0,
      decltype(v.public_fields())>::type first_field;

    static_assert('a' == T::annotation()[0],
      "type annotation must be constexpr");
    static_assert('a' == first_field::annotation()[0],
      "field annotation must be constexpr");

    return true;
  }
} constexpr_annotations;

static const struct first_field_type_name_t {
  template <typename T>
  std::string operator()(const T &v) const {
    const auto fields = v.public_fields();
    const auto f = std::get<0>(fields);
    typedef typename decltype(f)::type field_type;

    return omni::meta_t<field_type>::name();
  }
} first_field_type_name;

static const struct second_field_type_name_t {
  template <typename T>
  std::string operator()(const T &v) const {
    const auto fields = v.public_fields();
    const auto f = std::get<1>(fields);
    typedef typename decltype(f)::type field_type;

    return omni::meta_t<field_type>::name();
  }
} second_field_type_name;

static const struct enum_names_t {
  template <typename Enum>
  std::vector<std::string> operator()(const Enum &e) const {
    const auto enumerators = e.enumerators();
    std::vector<std::string> names;
    names.reserve(enumerators.size());

    for (std::size_t i = 0; enumerators.size() > i; ++i)
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
    const auto f = std::get<0>(v.public_fields());
    using field_type = typename decltype(f)::type;

    return std::string(v.name()) + "::"
      + std::string(omni::reflected<field_type>().name()) + ":int";
  }
} get_dependency_name;

static const struct get_dependency_name_layer_2_t {
  template <typename T>
  std::string operator()(const T &v) const {
    const auto outer_fields = v.public_fields();
    const auto outer_f = std::get<0>(outer_fields);
    using intermediate_type = typename decltype(outer_f)::type;

    const auto inner = outer_f.value();
    auto inner_reflection = omni::reflected(inner);
    const auto inner_fields = inner_reflection.public_fields();
    const auto inner_f = std::get<0>(inner_fields);
    using dep_type = typename decltype(inner_f)::type;

    return std::string(v.name()) + "::"
      + std::string(omni::reflected<intermediate_type>().name()) + "::"
      + std::string(omni::reflected<dep_type>().name()) + ":int";
  }
} get_dependency_name_layer_2;

} // namespace as_field

namespace as_alias {

static const struct get_dependency_name_t {
  template <typename Parent>
  std::string operator()(const Parent &) const {
    using parent_type = typename Parent::type;
    static_assert(omni::is_reflected<parent_type>::value,
      "Parent is not reflected");
    static_assert(omni::is_reflected<typename parent_type::value_type>::value,
      "Member alias is not reflected");

    return std::string(Parent::name()) + "::"
      + std::string(omni::meta_t<typename parent_type::value_type>::name())
      + ":int";
  }
} get_dependency_name;

static const struct get_dependency_name_layer_2_t {
  template <typename T>
  std::string operator()(const T &) const {
    using root_type = typename T::type;
    using level_1 = typename root_type::value_type;
    using dep_type = typename level_1::value_type;

    return std::string(T::name()) + "::"
      + std::string(omni::meta_t<level_1>::name()) + "::"
      + std::string(omni::meta_t<dep_type>::name()) + ":int";
  }
} get_dependency_name_layer_2;

} // namespace as_alias

namespace as_template_arg {

static const struct get_dependency_name_t {
  template <typename T>
  std::string operator()(const T &v) const {
    const auto fields = v.public_fields();
    const auto f = std::get<0>(fields);
    using tuple_type = typename decltype(f)::type;
    using dep_type = typename std::tuple_element<0, tuple_type>::type;

    return std::string(v.name()) + "::"
      + std::string(omni::meta_t<dep_type>::name()) + ":int";
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
    const auto outer_fields = v.public_fields();
    const auto outer_f = std::get<0>(outer_fields);
    using outer_field_type = typename decltype(outer_f)::type;

    using intermediate_type =
      typename first_variant_arg<outer_field_type>::type; // std::tuple<...>
    using dep_type = typename std::tuple_element<0, intermediate_type>::type;

    return std::string(v.name()) + "::tuple::" // intermediate_type is a std::tuple<...>
      + std::string(omni::meta_t<dep_type>::name()) + ":int";
  }
} get_dependency_name_layer_2;

} // namespace as_template_arg

namespace as_sequence_arg {

static const struct get_vector_value_name_t {
  template <typename T>
  std::string operator()(const T &v) const {
    const auto fields = v.public_fields();
    const auto f = std::get<0>(fields);
    typedef typename decltype(f)::type vector_type;
    typedef typename vector_type::value_type value_type;

    static_assert(omni::is_reflected<value_type>::value,
      "Vector value type is not reflected");

    return std::string(v.name())
      + "::vector::" + omni::meta_t<value_type>::name();
  }
} get_vector_value_name;

static const struct get_tuple_value_name_t {
  template <typename T>
  std::string operator()(const T &v) const {
    const auto fields = v.public_fields();
    const auto f = std::get<0>(fields);
    typedef typename decltype(f)::type tuple_type;
    typedef typename std::tuple_element<0, tuple_type>::type value_type;

    static_assert(omni::is_reflected<value_type>::value,
      "Tuple value type is not reflected");

    return std::string(v.name())
      + "::tuple::" + omni::meta_t<value_type>::name();
  }
} get_tuple_value_name;

static const struct get_tuple_second_value_name_t {
  template <typename T>
  std::string operator()(const T &v) const {
    const auto fields = v.public_fields();
    const auto f = std::get<0>(fields);
    typedef typename decltype(f)::type tuple_type;
    typedef typename std::tuple_element<1, tuple_type>::type value_type;

    static_assert(omni::is_reflected<value_type>::value,
      "Tuple second value type is not reflected");

    return std::string(v.name())
      + "::tuple::" + omni::meta_t<value_type>::name();
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
    const auto fields = v.public_fields();
    const auto f = std::get<0>(fields);
    typedef typename decltype(f)::type variant_type;
    typedef typename first_variant_arg<variant_type>::type value_type;

    static_assert(omni::is_reflected<value_type>::value,
      "Variant value type is not reflected");

    return std::string(v.name())
      + "::variant::" + omni::meta_t<value_type>::name();
  }
} get_variant_value_name;

static const struct get_nested_vector_tuple_value_name_t {
  template <typename T>
  std::string operator()(const T &v) const {
    const auto fields = v.public_fields();
    const auto f = std::get<0>(fields);
    typedef typename decltype(f)::type vector_type;
    typedef typename vector_type::value_type tuple_type;
    typedef typename std::tuple_element<0, tuple_type>::type value_type;

    static_assert(omni::is_reflected<value_type>::value,
      "Nested vector tuple value type is not reflected");

    return std::string(v.name())
      + "::vector::tuple::" + omni::meta_t<value_type>::name();
  }
} get_nested_vector_tuple_value_name;

} // namespace as_sequence_arg

namespace as_inherited_struct {

template <typename Base>
struct is_base_reflected_t {
  template <typename Derived>
  bool operator()(const Derived &) const {
    static_assert(std::is_base_of<Base, typename Derived::type>::value,
      "Input is not Derived from Base");
    return omni::is_reflected<Base>::value;
  }
};

} // namespace as_inherited_struct

// ---------- resolved dependency types ----------

namespace resolved {

/** annotation: as_field type */
struct as_field {
  //! annotation: as_field value
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

/*! annotation: inherited base type */
struct as_inherited_struct {
  int base_field; ///< annotation: inherited base field
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

//! annotation: scoped status enum
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

template <typename T>
struct custom_allocator: std::allocator<T> {
  using value_type = T;

  custom_allocator() noexcept {}

  template <typename U>
  custom_allocator(const custom_allocator<U> &) noexcept {}

  template <typename U>
  struct rebind {
    using other = custom_allocator<U>;
  };
};

// ---------- holders (dependency chains) ----------

struct example_value {
  std::string name;
  int count;
  double score;
};

struct unannotated_record {
  std::string name;
  int count;
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

struct enum_alias_dep {
  using value_type = scoped_status;
};

// tuple / variant dependency chains

struct template_dep_level_1 {
  std::tuple<resolved::as_template_arg> tpl_field_1;
};

struct enum_template_dep {
  std::tuple<scoped_fixed_status> tpl_field_1;
};

struct template_dep_level_2 {
  omni::compat::variant<std::tuple<resolved::as_template_arg_layer_2>>
    var_field_2;
};

// explicit test of third-party variant
struct mpark_template_dep_level_2 {
  mpark::variant<std::tuple<resolved::as_template_arg_layer_2>> var_field_2;
};

/** annotation: primary template record */
template <typename T>
struct primary_template_record {
  //! annotation: primary template value
  T value;
  int count; ///< annotation: primary template count
};

/*! annotation: comment forms type */
struct annotation_comment_forms {
  /// annotation: slash line field
  int slash_line;
  //! annotation: bang line field
  int bang_line;
  /** annotation: slash block field */
  int slash_block;
  /*! annotation: bang block field */
  int bang_block;
  int trailing_slash; ///< annotation: trailing slash field
  int trailing_bang; //!< annotation: trailing bang field
};

template <typename T, int N>
struct value_param_template_record {
  T value;
  int fixed_values[N];
};

template <typename T, typename Alloc = custom_allocator<T>>
struct default_allocator_template_record {
  std::vector<T, Alloc> values;
};

template <typename T, typename Alloc>
struct typed_allocator_template_record {
  std::vector<T, Alloc> values;
};

template <template <typename> class Alloc>
struct allocator_policy_template_record {
  std::vector<resolved::as_field, Alloc<resolved::as_field>> vec;
  std::map<int,
    resolved::as_second_base,
    std::less<int>,
    Alloc<std::pair<const int, resolved::as_second_base>>>
    map;
};

//! annotation: template base type
template <typename T>
struct template_base {
  /** annotation: template base value */
  T base_value;
};

/*! annotation: template derived type */
template <typename T>
struct template_derived: template_base<T> {
  int own_field; //!< annotation: template derived own field
};

/** annotation: CRTP base template */
template <typename Derived>
struct crtp_base {
  //! annotation: CRTP base field
  int crtp_base_field;
};

//! annotation: CRTP derived type
struct crtp_derived: crtp_base<crtp_derived> {
  std::string crtp_own_field; ///< annotation: CRTP own field
};

/// annotation: CRTP template derived type
template <typename T>
struct crtp_template_derived: crtp_base<crtp_template_derived<T>> {
  /// annotation: CRTP template field
  T crtp_template_field;
};

struct nested_template_parent {
  template <typename T>
  struct nested_template {
    T nested_value;
  };
};

template <typename T>
struct template_parent {
  struct nested_non_template {
    T nested_value;
    int count;
  };
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

struct forward_declarable_enum_holder {
  scoped_status scoped;
  fixed_status fixed;
  scoped_fixed_status scoped_fixed;
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
