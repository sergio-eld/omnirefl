#pragma once

#include <omnirefl/compat.hpp>

#include <type_traits>

namespace omni {
namespace traits {
namespace detail {

// Function templates cannot be partially specialized, so the public `is`
// overloads delegate template-shape matching to these class templates.
template <template <typename...> class, typename>
struct is_type_template_specialization: std::false_type {};

template <template <typename...> class Template, typename... Argument>
struct is_type_template_specialization<Template, Template<Argument...>>:
    std::true_type {};

#if defined(__cpp_nontype_template_parameter_auto)
template <template <typename, auto> class, typename>
struct is_type_value_template_specialization: std::false_type {};

template <template <typename, auto> class Template, typename Type, auto Value>
struct is_type_value_template_specialization<Template, Template<Type, Value>>:
    std::true_type {};
#endif

} // namespace detail

/// Whether `Type` is a specialization of a template whose parameters are
/// types.
template <template <typename...> class Template, typename Type>
constexpr bool is() noexcept {
  return detail::is_type_template_specialization<Template,
    compat::remove_cvref_t<Type>>::value;
}

#if defined(__cpp_nontype_template_parameter_auto)
/// Whether `Type` is a specialization of a template taking a type followed by
/// one non-type parameter.
template <template <typename, auto> class Template, typename Type>
constexpr bool is() noexcept {
  return detail::is_type_value_template_specialization<Template,
    compat::remove_cvref_t<Type>>::value;
}
#endif

} // namespace traits
} // namespace omni
