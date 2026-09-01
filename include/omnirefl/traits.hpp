#pragma once

#include <omnirefl/compat.hpp>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace omni {
namespace traits {
namespace detail {

// Function templates cannot be partially specialized, so the template
// overload of `traits::is` delegates equality to this trait.
template <template <typename...> class Left,
  template <typename...> class Right>
struct is_same_template: std::false_type {};

template <template <typename...> class Template>
struct is_same_template<Template, Template>: std::true_type {};

// Detects the result selected by the type-parameter-only `fn::ctad` overload.
// Valid specializations expose the result as `type`.
template <template <typename...> class Template,
  typename Value,
  typename = void>
struct type_template_construct_result: std::false_type {};

#if defined(__cpp_deduction_guides) && 201703L <= __cpp_deduction_guides
template <template <typename...> class Template, typename Value>
struct type_template_construct_result<Template,
  Value,
  compat::void_t<decltype(Template{std::declval<Value>()})>>: std::true_type {
  using type = decltype(Template{std::declval<Value>()});
};
#else
template <template <typename...> class Template, typename Value>
struct type_template_construct_result<Template,
  Value,
  compat::void_t<decltype(Template<compat::decay_t<Value>>{
    std::declval<Value>()})>>: std::true_type {
  using type = Template<compat::decay_t<Value>>;
};
#endif

// Detects the CTAD result selected by the type-and-size `fn::ctad` overload.
// It remains invalid when deduction guides are unavailable.
template <template <typename, std::size_t> class Template,
  typename Value,
  typename = void>
struct type_size_template_construct_result: std::false_type {};

#if defined(__cpp_deduction_guides) && 201703L <= __cpp_deduction_guides
template <template <typename, std::size_t> class Template, typename Value>
struct type_size_template_construct_result<Template,
  Value,
  compat::void_t<decltype(Template{std::declval<Value>()})>>: std::true_type {
  using type = decltype(Template{std::declval<Value>()});
};
#endif

} // namespace detail

/// Report whether `To` can be brace-constructed from `Value`.
template <typename To, typename Value, typename = void>
struct is_brace_constructible: std::false_type {};

template <typename To, typename Value>
struct is_brace_constructible<To,
  Value,
  compat::void_t<decltype(To{std::declval<Value>()})>>: std::true_type {};

/// Report whether a type-parameter-only class template can construct from
/// `Value` using CTAD or the pre-C++17 value-type fallback.
template <template <typename...> class Template, typename Value>
struct is_type_template_constructible_from:
    detail::type_template_construct_result<Template, Value> {};

/// The result selected by `is_type_template_constructible_from`.
template <template <typename...> class Template, typename Value>
using type_template_construct_result_t =
  typename detail::type_template_construct_result<Template, Value>::type;

/// Report whether a type-and-size class template can use CTAD with `Value`.
template <template <typename, std::size_t> class Template, typename Value>
struct is_type_size_template_constructible_from:
    detail::type_size_template_construct_result<Template, Value> {};

/// The result selected by `is_type_size_template_constructible_from`.
template <template <typename, std::size_t> class Template, typename Value>
using type_size_template_construct_result_t =
  typename detail::type_size_template_construct_result<Template, Value>::type;

#if defined(__cpp_concepts) && 201907L <= __cpp_concepts
/// Require brace construction of `To` from `Value`.
template <typename To, typename Value>
concept brace_constructible_from = is_brace_constructible<To, Value>::value;

/// Require a valid type-parameter-only template construction from `Value`.
template <template <typename...> class Template, typename Value>
concept type_template_constructible_from =
  is_type_template_constructible_from<Template, Value>::value;

/// Require valid CTAD for a type-and-size template from `Value`.
template <template <typename, std::size_t> class Template, typename Value>
concept type_size_template_constructible_from =
  is_type_size_template_constructible_from<Template, Value>::value;
#endif

/// Compare two types.
template <typename Left, typename Right>
constexpr bool is() {
  return std::is_same<Left, Right>::value;
}

/// Compare two templates containing only type template parameters.
template <template <typename...> class Left,
  template <typename...> class Right>
constexpr bool is() {
  return detail::is_same_template<Left, Right>::value;
}

} // namespace traits
} // namespace omni
